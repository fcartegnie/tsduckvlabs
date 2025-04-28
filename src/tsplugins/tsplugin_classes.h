#include "../plugins/plugins/tsAbstractTablePlugin.h"
#include "../dtv/signalization/tsServiceDiscovery.h"
#include "../dtv/signalization/tsAudioLanguageOptions.h"
#include "../dtv/transport/tsTSPacketFormat.h"
#include "../dtv/transport/tsTSForkPipe.h"
#include "../dtv/transport/tsTSPacketQueue.h"
#include "../dtv/timing/tsPCRMerger.h"
#include "../dtv/signalization/tsPSIMerger.h"
#include "../dtv/timing/tsPacketInsertionController.h"
#include "../dtv/timing/tsTSSpeedMetrics.h"
#include "../base/system/tsFileNameGenerator.h"
#include "../base/algo/tsSingleDataStatistics.h"
#include "../base/system/tsFileUtils.h"
#include "../dtv/pes/tsPESHandlerInterface.h"
#include "../dtv/demux/tsTableHandlerInterface.h"
#include "../dtv/pes/tsPESDemux.h"

namespace ts {
    class PMTPlugin: public AbstractTablePlugin
    {
        TS_PLUGIN_CONSTRUCTORS(PMTPlugin);
    public:
        // Implementation of plugin API
        virtual bool start() override;
        virtual Status processPacket(TSPacket&, TSPacketMetadata&) override;

    private:
        // Description of a new component to add
        struct NewPID {
            PID     pid;
            uint8_t type;

            // Constructor.
            NewPID(PID pid_ = PID_NULL, uint8_t stype_ = 0) :
                pid(pid_),
                type(stype_)
            {
            }
        };

        // Map of new descriptors to add per component.
        using DescriptorListPtr = std::shared_ptr<DescriptorList>;
        using DescriptorListByPID = std::map<PID, DescriptorListPtr>;

        // PMTPlugin instance fields
        ServiceDiscovery     _service {duck, nullptr};   // Service of PMT to modify
        std::vector<PID>     _removed_pids {};           // Set of PIDs to remove from PMT
        std::vector<DID>     _removed_desc_tags {};      // Set of descriptor tags to remove
        std::vector<uint8_t> _removed_stream_types {};   // Set of stream types to remove
        std::list<NewPID>    _added_pids {};             // List of PID to add
        std::map<PID,PID>    _moved_pids {};             // List of renamed PID's in PMT (key=old, value=new)
        bool                 _set_servid = false;        // Set a new service id
        uint16_t             _new_servid = 0;            // New service id
        bool                 _set_pcrpid = false;        // Set a new PCR PID
        PID                  _new_pcrpid = PID_NULL;     // New PCR PID
        PDS                  _pds = 0;                   // Private data specifier for removed descriptors
        bool                 _add_stream_id = false;     // Add stream_identifier_descriptor on all components
        bool                 _ac3_atsc2dvb = false;      // Modify AC-3 signaling from ATSC to DVB method
        bool                 _eac3_atsc2dvb = false;     // Modify Enhanced-AC-3 signaling from ATSC to DVB method
        bool                 _cleanup_priv_desc = false; // Remove private desc without preceding PDS desc
        DescriptorList       _add_descs {nullptr};       // List of descriptors to add at program level
        DescriptorListByPID  _add_pid_descs {};          // Lists of descriptors to add by PID
        AudioLanguageOptionsVector _languages {};        // Audio languages to set
        std::vector<PID>     _sort_pids {};              // Sorting order of PIDs in PMT
        UStringVector        _sort_languages {};         // Sorting order of audio and subtitles PIDs in PMT

        // Implementation of AbstractTablePlugin.
        virtual void createNewTable(BinaryTable& table) override;
        virtual void modifyTable(BinaryTable& table, bool& is_target, bool& reinsert) override;

        // Add a descriptor for a given PID in _add_pid_descs.
        void addComponentDescriptor(PID pid, const AbstractDescriptor& desc);

        // Decode an option "pid/value[/hexa]". Hexa is allowed only if hexa is non zero.
        template<typename INT>
        bool decodeOptionForPID(const UChar* parameter_name, size_t parameter_index, PID& pid, INT& value, ByteBlock* hexa = nullptr, INT value_max = std::numeric_limits<INT>::max());

        // Decode options like --set-stream-identifier which add a simple descriptor in a component.
        template<typename DESCRIPTOR, typename INT>
        bool decodeComponentDescOption(const UChar* parameter_name);
    };

#define DEFAULT_MAX_QUEUED_PACKETS  1000            // Default size in packet of the inter-thread queue.
#define SERVER_THREAD_STACK_SIZE    (128 * 1024)    // Size in byte of the thread stack.

    class MergePlugin: public ProcessorPlugin, private Thread
    {
        TS_PLUGIN_CONSTRUCTORS(MergePlugin);
    public:
        // Implementation of plugin API
        virtual bool getOptions() override;
        virtual bool start() override;
        virtual bool stop() override;
        virtual Status processPacket(TSPacket&, TSPacketMetadata&) override;

    private:
        // Command line options.
        UString          _command {};                                       // Command which generates the main stream.
        TSPacketFormat   _format = TSPacketFormat::AUTODETECT;              // Packet format on the pipe
        size_t           _max_queue = DEFAULT_MAX_QUEUED_PACKETS;           // Maximum number of queued packets.
        size_t           _accel_threshold = DEFAULT_MAX_QUEUED_PACKETS / 2; // Queue threshold after which insertion is accelerated.
        bool             _no_wait = false;              // Do not wait for command completion.
        bool             _merge_psi = false;            // Merge PSI/SI information.
        bool             _pcr_restamp = false;          // Restamp PCR from the merged stream.
        bool             _incremental_pcr = false;      // Use incremental method to restamp PCR's.
        bool             _merge_smoothing = false;      // Smoothen packet insertion.
        bool             _ignore_conflicts = false;     // Ignore PID conflicts.
        bool             _pcr_reset_backwards = false;  // Reset PCR restamping when DTS/PTD move backwards the PCR.
        bool             _terminate = false;            // Terminate processing after last merged packet.
        bool             _restart = false;              // Restart command after termination.
        cn::milliseconds _restart_interval {};          // Interval before restarting the merge command.
        BitRate          _user_bitrate = 0;             // User-specified bitrate of the merged stream.
        PIDSet           _allowed_pids {};              // List of PID's to merge (other PID's from the merged stream are dropped).
        TSPacketLabelSet _set_labels {};                // Labels to set on output packets.
        TSPacketLabelSet _reset_labels {};              // Labels to reset on output packets.

        // The ForkPipe is dynamically allocated to avoid reusing the same object when the command is restarted.
        using TSForkPipePtr = std::shared_ptr<TSForkPipe>;

        // Working data.
        bool          _got_eof = false;    // Got end of merged stream.
        volatile bool _stopping = false;   // Plugin stop in progress.
        PacketCounter _merged_count = 0;   // Number of merged packets.
        PacketCounter _hold_count = 0;     // Number of times we didn't try to merge to perform smoothing insertion.
        PacketCounter _empty_count = 0;    // Number of times we could merge but there was no packet to merge.
        TSForkPipePtr _pipe {};            // Executed command.
        TSPacketQueue _queue {};           // TS packet queur from merge to main.
        PIDSet        _main_pids {};       // Set of detected PID's in main stream.
        PIDSet        _merge_pids {};      // Set of detected PID's in merged stream that we pass in main stream.
        PCRMerger     _pcr_merger {duck};  // Adjust PCR's in merged stream.
        PSIMerger     _psi_merger {duck, PSIMerger::NONE};  // Used to merge PSI/SI from both streams.
        PacketInsertionController _insert_control {*this};  // Used to control insertion points for the merge

        // Start/restart/stop the merge command.
        bool startStopCommand(bool do_close, bool do_start);

        // There is one thread which receives packet from the created process and passes
        // them to the main plugin thread. The following method is the thread main code.
        virtual void main() override;

        // Process one packet coming from the merged stream.
        Status processMergePacket(TSPacket&, TSPacketMetadata&);
    };

    class StatsPlugin: public ProcessorPlugin
    {
        TS_PLUGIN_CONSTRUCTORS(StatsPlugin);
    public:
        // Implementation of plugin API
        virtual bool getOptions() override;
        virtual bool start() override;
        virtual bool stop() override;
        virtual Status processPacket(TSPacket&, TSPacketMetadata&) override;

    private:
        // Each category of packets (PID or lable) is described by a structure like this.
        // The map is indexed by PID or label.
        class Context;
        using ContextPtr = std::shared_ptr<Context>;
        using ContextMap = std::map<size_t, ContextPtr>;

        // Command line options.
        bool             _track_pids = true;       // Track PID's, not labels.
        bool             _log = false;             // Report statistics through the logger, not files.
        bool             _csv = false;             // Use CSV format for statistics.
        bool             _header = false;          // Display header lines.
        bool             _multiple_output = false; // Don't rewrite output files with --interval.
        UString          _csv_separator {DEFAULT_CSV_SEPARATOR}; // Separator character in CSV lines.
        fs::path         _output_name {};          // Output file name.
        cn::nanoseconds  _output_interval {};      // Recreate output at this time interval.
        PIDSet           _pids {};                 // List of PID's to track.
        TSPacketLabelSet _labels {};               // List of labels to track.

        // Working data.
        std::ofstream     _output_stream {};  // Output file stream.
        std::ostream*     _output = nullptr;  // Point to actual output stream.
        ContextMap        _ctx_map {};        // Description of all tracked categories of packets.
        TSSpeedMetrics    _metrics {};        // Timing to synchronize next output files.
        cn::nanoseconds   _next_report {};   // Next time to create next output.
        FileNameGenerator _name_gen {};       // Generate multiple output file names.

        // Get or create the description of a tracked PID or label.
        ContextPtr getContext(size_t index);

        // Open, close and create statistics report.
        bool openOutput();
        void closeOutput();
        bool produceReport();

        // Description of a tracked category of packet (PID or label).
        class Context
        {
        public:
            Context() = default;         // Constructor.
            uint64_t total_pkt = 0;      // Total number of packets in that category.
            uint64_t last_ts_index = 0;  // Index in TS of last packet of the category.
            SingleDataStatistics<uint64_t> ipkt {}; // Inter-packet distance statistics.

            // Add packet data to the context.
            void addPacketData(PacketCounter, const TSPacket&);
        };
    };

    class FilterPlugin: public ProcessorPlugin, private SignalizationHandlerInterface
    {
        TS_PLUGIN_CONSTRUCTORS(FilterPlugin);
    public:
        // Implementation of plugin API
        virtual bool getOptions() override;
        virtual bool start() override;
        virtual bool stop() override;
        virtual Status processPacket(TSPacket&, TSPacketMetadata&) override;

    private:
        // Packet intervals and list of them.
        using PacketRange = std::pair<PacketCounter, PacketCounter>;
        using PacketRangeList = std::list<PacketRange>;

        // Command line options:
        Status             _drop_status = TSP_DROP;     // Return status for unselected packets
        int                _scrambling_ctrl = 0;        // Scrambling control value (<0: no filter)
        bool               _need_demux = false;         // Need the help of the signalization demux.
        bool               _with_payload = false;       // Packets with payload
        bool               _with_af = false;            // Packets with adaptation field
        bool               _with_pes = false;           // Packets with clear PES headers
        bool               _with_pcr = false;           // Packets with PCR or OPCR
        bool               _with_splice = false;        // Packets with splice_countdown in adaptation field
        bool               _unit_start = false;         // Packets with payload unit start
        bool               _intra_frame = false;        // Packets with start of video intra-frame
        bool               _nullified = false;          // Packets which were nullified by a previous plugin
        bool               _input_stuffing = false;     // Null packets which were artificially inserted
        bool               _valid = false;              // Packets with valid sync byte and error ind
        bool               _negate = false;             // Negate filter (exclude selected packets)
        bool               _video = false;              // Part of a video PID
        bool               _audio = false;              // Part of an audio PID
        bool               _subtitles = false;          // Part of a subtitles PID
        bool               _ecm = false;                // Part of an ECM PID
        bool               _emm = false;                // Part of an EMM PID
        bool               _psi = false;                // Part of global PSI/SI PID.
        int                _min_payload = 0;            // Minimum payload size (<0: no filter)
        int                _max_payload = 0;            // Maximum payload size (<0: no filter)
        int                _min_af = 0;                 // Minimum adaptation field size (<0: no filter)
        int                _max_af = 0;                 // Maximum adaptation field size (<0: no filter)
        int                _splice = 0;                 // Exact splice_countdown value (<-128: no filter)
        int                _min_splice = 0;             // Minimum splice_countdown value (<-128: no filter)
        int                _max_splice = 0;             // Maximum splice_countdown value (<-128: no filter)
        PacketCounter      _after_packets = 0;          // Number of initial packets to skip
        PacketCounter      _every_packets = 0;          // Filter 1 out of this number of packets
        CodecType          _codec = CodecType::UNDEFINED; // Filter on codec type
        PIDSet             _explicit_pid {};            // Explicit PID values to filter
        ByteBlock          _pattern {};                 // Byte pattern to search.
        bool               _search_payload = false;     // Search pattern in payload only
        bool               _use_search_offset = false;  // Search at specified offset only
        size_t             _search_offset = 0;          // Offset where to search.
        PacketRangeList    _ranges {};                  // Ranges of packets to filter.
        std::set<uint8_t>  _stream_ids {};              // PES stream ids to filter
        std::set<uint16_t> _service_ids {};             // Service ids to filter
        UStringVector      _service_names {};           // Service names to filter.
        TSPacketLabelSet   _labels {};                  // Select packets with any of these labels
        TSPacketLabelSet   _set_labels {};              // Labels to set on filtered packets
        TSPacketLabelSet   _reset_labels {};            // Labels to reset on filtered packets
        TSPacketLabelSet   _set_perm_labels {};         // Labels to set on all packets after getting one packet
        TSPacketLabelSet   _reset_perm_labels {};       // Labels to reset on all packets after getting one packet

        // Working data:
        PacketCounter      _filtered_packets = 0;       // Number of filtered packets
        PIDSet             _stream_id_pid {};           // PID values selected from stream ids
        std::set<uint16_t> _all_service_ids {};         // All service ids to filter, after service name resolution
        SignalizationDemux _demux {duck};               // Full signalization demux

        // Implementation of SignalizationHandlerInterface
        virtual void handleService(uint16_t ts_id, const Service& service, const PMT& pmt, bool removed) override;
    };

    class PESPlugin: public ProcessorPlugin, private PESHandlerInterface
    {
        TS_PLUGIN_CONSTRUCTORS(PESPlugin);
    public:
        // Implementation of plugin API
        virtual bool getOptions() override;
        virtual bool start() override;
        virtual bool stop() override;
        virtual Status processPacket(TSPacket&, TSPacketMetadata&) override;

    private:
        // Commmand line options.
        bool      _trace_packets = false;
        bool      _trace_packet_index = false;
        bool      _dump_pes_header = false;
        bool      _dump_pes_payload = false;
        bool      _dump_start_code = false;
        bool      _dump_nal_units = false;
        bool      _dump_avc_sei = false;
        bool      _video_attributes = false;
        bool      _audio_attributes = false;
        bool      _intra_images = false;
        bool      _negate_nal_unit_filter = false;
        bool      _multiple_files = false;
        bool      _flush_last = false;
        uint32_t  _hexa_flags = 0;
        size_t    _hexa_bpl = 0;
        size_t    _max_dump_size = 0;
        size_t    _max_dump_count = 0;
        int       _min_payload = 0;    // Minimum payload size (<0: no filter)
        int       _max_payload = 0;    // Maximum payload size (<0: no filter)
        fs::path  _out_filename {};
        fs::path  _pes_filename {};
        fs::path  _es_filename {};
        PIDSet    _pids {};
        CodecType _default_h26x = CodecType::UNDEFINED;
        std::set<uint8_t>    _nal_unit_filter {};
        std::set<uint32_t>   _sei_type_filter {};
        std::list<ByteBlock> _sei_uuid_filter {};

        // Working data.
        bool              _abort = false;
        std::ofstream     _out_file {};
        std::ostream*     _out = nullptr;
        std::ofstream     _pes_file {};
        std::ostream*     _pes_stream = nullptr;
        std::ofstream     _es_file {};
        std::ostream*     _es_stream = nullptr;
        PESDemux          _demux;
        FileNameGenerator _pes_name_gen {};
        FileNameGenerator _es_name_gen {};

        // Open output file.
        bool openOutput(const fs::path&, std::ofstream*, std::ostream**, bool binary);

        // A string containing the PID and optional TS packet indexes.
        UString prefix(const DemuxedData&) const;

        // Do we need to display this acces unit type?
        bool useAccesUnitType(uint8_t) const;

        // Process dump count. Return true when terminated. Also process error on output.
        bool lastDump(std::ostream&);

        // Save one file using --multiple-file. Set _abort on error.
        void saveOnePES(FileNameGenerator& namegen, const uint8_t* data, size_t size);

        // Implementation of PESHandlerInterface.
        virtual void handlePESPacket(PESDemux&, const PESPacket&) override;
        virtual void handleInvalidPESPacket(PESDemux&, const DemuxedData&) override;
        virtual void handleIntraImage(PESDemux&, const PESPacket&, size_t) override;
        virtual void handleVideoStartCode(PESDemux&, const PESPacket&, uint8_t, size_t, size_t) override;
        virtual void handleNewMPEG2VideoAttributes(PESDemux&, const PESPacket&, const MPEG2VideoAttributes&) override;
        virtual void handleAccessUnit(PESDemux&, const PESPacket&, uint8_t, size_t, size_t) override;
        virtual void handleSEI(PESDemux& demux, const PESPacket& packet, uint32_t sei_type, size_t offset, size_t size) override;
        virtual void handleNewAVCAttributes(PESDemux&, const PESPacket&, const AVCAttributes&) override;
        virtual void handleNewHEVCAttributes(PESDemux&, const PESPacket&, const HEVCAttributes&) override;
        virtual void handleNewMPEG2AudioAttributes(PESDemux&, const PESPacket&, const MPEG2AudioAttributes&) override;
        virtual void handleNewAC3Attributes(PESDemux&, const PESPacket&, const AC3Attributes&) override;
    };
}
