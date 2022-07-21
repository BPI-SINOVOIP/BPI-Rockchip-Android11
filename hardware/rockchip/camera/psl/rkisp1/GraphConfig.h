/*
 * Copyright (C) 2015-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _CAMERA3_GRAPHCONFIG_H_
#define _CAMERA3_GRAPHCONFIG_H_

#include <string>
#include <memory>
#include <vector>
#include <set>
#include <utils/Errors.h>
#include <hardware/camera3.h>
#include <gcss.h>
/* #include <rk_aiq.h> */
#include <linux/media.h>
#include "MediaCtlPipeConfig.h"
#include "LogHelper.h"
#include "MediaController.h"
#include "RKISP1CameraCapInfo.h"

namespace GCSS {
class GraphConfigNode;
}

// mainPath output capacity
#define MP_MAX_WIDTH        4416
#define MP_MAX_HEIGHT       3312
// selfPath output capacity
#define SP_MAX_WIDTH        1920
#define SP_MAX_HEIGHT       1080
// postpipeline limitation, limited by RGA now
#if defined(TARGET_RK312X)
#define PP_MAX_WIDTH        2048
#else
#define PP_MAX_WIDTH        4096
#endif

#define NODE_NAME(x) (getNodeName(x).c_str())

namespace android {
namespace camera2 {

class GraphConfigManager;

const int32_t ACTIVE_ISA_OUTPUT_BUFFER = 2;
const int32_t MAX_STREAMS = 4; // max number of streams
const uint32_t MAX_KERNEL_COUNT = 30; // max number of kernels in the kernel list
const std::string SENSOR_PORT_NAME("sensor:port_0");
const std::string TPG_PORT_NAME("tpg:port_0");
// Declare string consts
const std::string CSI_BE = "rockchip-mipi-dphy-rx";

const std::string GC_INPUT = "input";
const std::string GC_OUTPUT = "output";
const std::string GC_PREVIEW = "preview";
const std::string GC_VIDEO = "video";
const std::string GC_STILL = "still";
const std::string GC_RAW = "raw";

/**
 * Stream id associated with the ISA PG that runs on Psys.
 */
static const int32_t PSYS_ISA_STREAM_ID = 60002;
/**
 * Stream id associated with the ISA PG that runs on Isys.
 */
static const int32_t ISYS_ISA_STREAM_ID = 0;

/**
 * \struct SinkDependency
 *
 * This structure stores dependency information for each virtual sink.
 * This information is useful to determine the connections that preceded the
 * virtual sink.
 * We do not go all the way up to the sensor (we could), we just store the
 * terminal id of the input port of the pipeline that serves a particular sink
 * (i.e. the input port of the video pipe or still pipe)
 */
struct SinkDependency {
    uid_t sinkGCKey;       /**< GCSS_KEY that represents a sink, like GCSS_KEY_VIDEO1 */
    int32_t streamId;   /**< (a.k.a pipeline id) linked to this sink (ex 60000) */
    uid_t streamInputPortId;    /**< 4CC code of that terminal */

    SinkDependency():
        sinkGCKey(0),
        streamId(-1),
        streamInputPortId(0) {};
};
/**
 * \class GraphConfig
 *
 * Reference and accessor to pipe configuration for specific request.
 *
 * In general case, at sream-config time there are multiple possible graphs.
 * Per each request there is additional intent that can narrow down the
 * possibilities to single graph settings: the GraphConfig object.
 *
 * This class is instantiated by \class GraphConfigManager for each request,
 * and passed around HAL (control unit, capture unit, processing unit) via
 * shared pointers. The objects are read-only and owned by GCM.
 */
class GraphConfig {
public:
    typedef GCSS::GraphConfigNode Node;
    typedef std::vector<Node*> NodesPtrVector;
    typedef std::vector<int32_t> StreamsVector;
    typedef std::map<int32_t, int32_t> StreamsMap;
    typedef std::map<camera3_stream_t*, uid_t> StreamToSinkMap;
    static const int32_t PORT_DIRECTION_INPUT = 0;
    static const int32_t PORT_DIRECTION_OUTPUT = 1;

    class ConnectionConfig {
    public:
        ConnectionConfig(): mSourceStage(0),
                            mSourceTerminal(0),
                            mSourceIteration(0),
                            mSinkStage(0),
                            mSinkTerminal(0),
                            mSinkIteration(0) {}

        ConnectionConfig(uint32_t sourceStage,
                         uint32_t sourceTerminal,
                         uint32_t sourceIteration,
                         uint32_t sinkStage,
                         uint32_t sinkTerminal,
                         uint32_t sinkIteration):
                            mSourceStage(sourceStage),
                            mSourceTerminal(sourceTerminal),
                            mSourceIteration(sourceIteration),
                            mSinkStage(sinkStage),
                            mSinkTerminal(sinkTerminal),
                            mSinkIteration(sinkIteration) {}
        void dump() {
            LOGE("connection src 0x%x (0x%x) sink 0x%x(0x%x)",
                    mSourceStage, mSourceTerminal,
                    mSinkStage, mSinkTerminal);
        }

        uint32_t mSourceStage;
        uint32_t mSourceTerminal;
        uint32_t mSourceIteration;
        uint32_t mSinkStage;
        uint32_t mSinkTerminal;
        uint32_t mSinkIteration;
    };
   /**
    * \struct PortFormatSettings
    * Format settings for a port in the graph
    */
    struct PortFormatSettings {
        int32_t      enabled;
        uint32_t     terminalId; /**< Unique terminal id (is a fourcc code) */
        int32_t      width;    /**< Width of the frame in pixels */
        int32_t      height;   /**< Height of the frame in lines */
        int32_t      fourcc;   /**< Frame format */
        int32_t      bpl;      /**< Bytes per line*/
        int32_t      bpp;      /**< Bits per pixel */
    };

   /**
    * \struct PSysPipelineConnection
    * Group port format, connection, stream, edge port for
    * pipeline configuration
    */
    struct PSysPipelineConnection {
        PortFormatSettings portFormatSettings;
        ConnectionConfig connectionConfig;
        camera3_stream_t *stream;
        bool hasEdgePort;
    };

public:
     GraphConfig();
    ~GraphConfig();

    /*
     * Convert Node to GraphConfig interface
     */
    const GCSS::IGraphConfig* getInterface(Node *node) const;
    const GCSS::IGraphConfig* getInterface() const;
    bool hasStreamInGraph(int streamId);
    bool isKernelInStream(uint32_t streamId, uint32_t kernelId);
    status_t getPgIdForKernel(const uint32_t streamId, const int32_t kernelId,
                              int32_t &pgId);
    int32_t getTuningMode(int32_t streamId);
    /*
     * Graph Interrogation methods
     */
    status_t graphGetSinksByName(const std::string &name, NodesPtrVector &sinks);
    status_t graphGetDimensionsByName(const std::string &name,
                                              int &widht, int &height);
    status_t graphGetDimensionsByName(const std::string &name,
                                              unsigned short &widht, unsigned short &height);
   /*
    * Find distinct stream ids from the graph
    */
    status_t graphGetStreamIds(std::vector<int32_t> &streamIds);
    /*
     * Sink Interrogation methods
     */
    int32_t sinkGetStreamId(Node *sink);
    int32_t portGetStreamId(Node *port);
    /*
     * Stream Interrogation methods
     */
    status_t streamGetProgramGroups(int32_t streamId,
                                    NodesPtrVector &programGroups);
    status_t streamGetInputPort(int32_t streamId, Node **port);
    status_t streamGetConnectedOutputPorts(int32_t streamId,
                                           NodesPtrVector &outputPorts,
                                           NodesPtrVector &peerPorts);
    /*
     * Port Interrogation methods
     */
    status_t portGetFullName(Node *port, std::string &fullName);
    status_t portGetPeer(Node *port, Node **peer);
    status_t portGetFormat(Node *port, PortFormatSettings &format);
    status_t portGetConnection(Node *port,
                               ConnectionConfig &connectionInfo,
                               Node **peerPort);
    status_t portGetClientStream(Node *port, camera3_stream_t **stream);
    int32_t portGetDirection(Node *port);
    bool portIsVirtual(Node *port);
    status_t portGetPeerIdByName(std::string name,
                                 uid_t &terminalId);
    bool isPipeEdgePort(Node *port); // TODO: should be renamed as portIsEdgePort
    status_t getDimensions(const Node *node, int &w, int &h) const;
    status_t getDimensions(const Node *node, int &w, int &h, int &l,int &t) const;
    /*
     * ISA support methods
     */
    int32_t getIsaOutputCount() const;
    status_t getActiveDestinations(std::vector<uid_t> &terminalIds) const;
    bool isIsaOutputDestinationActive(uid_t destinationPortId) const;
    bool isIsaStreamActive(int32_t streamId) const;

    bool getSensorEmbeddedMetadataEnabled() const { return mMetaEnabled; }
    /*
     * Pipeline connection support
     */
    status_t pipelineGetInternalConnections(
                        const std::string &sinkName,
                        int &streamId,
                        std::vector<PSysPipelineConnection> &confVector);
    /*
     * Phase-in control
     */
    bool isFallback() const { return mFallback; }
    /*
     * re-cycler static method
     */
    static void reset(GraphConfig *me);
    void fullReset();
    /*
     * Debugging support
     */
    void dumpSettings();
    void dumpKernels(int32_t streamId);
    std::string getNodeName(Node *node);
    status_t getValue(string &nodeName, uint32_t id, int &value);
    bool doesNodeExist(string nodeName);

    status_t getIsaStreamIds(std::vector<int32_t> &streamIdVector,
                             std::map<string, int32_t> &isaOutputPort2StreamIdMap);
    enum PipeType {
        PIPE_STILL = 0,
        PIPE_PREVIEW,
    };
    PipeType getPipeType() const { return mPipeType; }
    void setPipeType(PipeType type) { mPipeType = type; }
    bool isStillPipe() { return mPipeType == PIPE_STILL; }

public:
    void setMediaCtlConfig(std::shared_ptr<MediaController> mediaCtl,
                           bool swapVideoPreview,
                           bool enableStill);

private:
    /* Helper structures to access Sensor Node information easily */
    class Rectangle {
    public:
        Rectangle();
        int32_t w;  /*<! width */
        int32_t h;  /*<! height */
        int32_t t;  /*<! top */
        int32_t l;  /*<! left */
    };

    struct MediaCtlLut {
        string uidStr;
        uint32_t uid;
        int pad;
        string nodeName;
        int nodeType;
    };

    class SubdevPad: public Rectangle {
    public:
        SubdevPad();
        int32_t mbusFormat;
    };
    struct BinFactor {
        int32_t h;
        int32_t v;
    };
    struct ScaleFactor {
        int32_t num;
        int32_t denom;
    };
    union RcFactor {  // Resolution Changing factor
        BinFactor bin;
        ScaleFactor scale;
    };
    struct SubdevInfo {
        string name;
        SubdevPad in;
        SubdevPad out;
        RcFactor factor;
    };
    class SourceNodeInfo {
    public:
        SourceNodeInfo();
        string name;
        string i2cAddress;
        string modeId;
        bool metadataEnabled;
        string csiPort;
        string nativeBayer;
        SubdevInfo tpg;
        SubdevInfo pa;
        SubdevPad output;
        int32_t interlaced;
        string verticalFlip;
        string horizontalFlip;
        string link_freq;
        bool dvp;
    };
    friend class GraphConfigManager;
    // Private initializer: only used by our friend GraphConfigManager.
    void init(int32_t reqId);
    status_t prepare(GraphConfigManager *manager,
                     Node *settings,
                     StreamToSinkMap &streamToSinkIdMap,
                     bool fallback);
    status_t prepare(GraphConfigManager *manager,
                     StreamToSinkMap &streamToSinkIdMap);
    status_t analyzeSourceType();
    void setActiveSinks(std::vector<uid_t> &activeSinks);
    void setActiveStreamId(const std::vector<uid_t> &activeSinks);
    void calculateSinkDependencies();
    void storeTuningModes();

    /*
     * Helpers for constructing mediaCtlConfigs from graph config
     */
    status_t parseSensorNodeInfo(Node* sensorNode, SourceNodeInfo &info);
    status_t parseTPGNodeInfo(Node* tpgNode, SourceNodeInfo &info);

    /*
     * legacy func to get MediaCtlConfig by parsing graph_setting_xx.xml
     */
    status_t getMediaCtlData(MediaCtlConfig* mediaCtlConfig);
    status_t getImguMediaCtlData(int32_t cameraId,
                                 int32_t testPatternMode,
                                 MediaCtlConfig* mediaCtlConfig,
                                 MediaCtlConfig* mediaCtlConfigVideo,
                                 MediaCtlConfig* mediaCtlConfigStill);
    /*
     * new func to get MediaCtlConfig, no need graph_setting_xx.xml anymore
     * the MediaCtlConfig is generated according to the app streams and sensor
     * available output frame size
     */
    void cal_crop(uint32_t &src_w, uint32_t &src_h, uint32_t &dst_w, uint32_t &dst_h);
    status_t selectSensorOutputFormat(int32_t cameraId, int &w, int &h, uint32_t &format);
    string getSinkEntityName(std::shared_ptr<MediaEntity> entity, int port);
    status_t getSensorMediaCtlConfig(int32_t cameraId,
                                 int32_t testPatternMode,
                                 MediaCtlConfig* mediaCtlConfig);
    status_t getImguMediaCtlConfig(int32_t cameraId,
                                 int32_t testPatternMode,
                                 MediaCtlConfig* mediaCtlConfig,
                                 std::vector<camera3_stream_t*>& outputStream);
    status_t addControls(const Node *sensorNode,
                         const SourceNodeInfo &sensorInfo,
                         MediaCtlConfig* config);

    void addVideoNodes(const Node* csiBESocOutput,
                       MediaCtlConfig* config);
    void addImguVideoNode(int nodeType, const string& nodeName, MediaCtlConfig* config);
    status_t getBinningFactor(const Node *node,
                              int32_t &hBin, int32_t &vBin) const;
    status_t getScalingFactor(const Node *node,
                              int32_t &scalingNum,
                              int32_t &scalingDenom) const;
    void addCtlParams(const string &entityName,
                      uint32_t controlName,
                      int controlId,
                      const string &strValue,
                      MediaCtlConfig* config);
    void addFormatParams(const string &entityName,
                         int width,
                         int height,
                         int pad,
                         int formatCode,
                         int field,
                         int quantization,
                         MediaCtlConfig* config);
    void addLinkParams(const string &srcName,
                       int srcPad,
                       const string &sinkName,
                       int sinkPad,
                       int enable,
                       int flags,
                       MediaCtlConfig* config);
    void addSelectionParams(const string &entityName,
                            int width,
                            int height,
                            int left,
                            int top,
                            int target,
                            int pad,
                            MediaCtlConfig* config);
    void addSelectionVideoParams(const string &entityName,
                                 const struct v4l2_selection &select,
                                 MediaCtlConfig* config);
    status_t getNodeInfo(const ia_uid uid, const Node &parent, int *width, int *height);
    status_t getTPGMediaCtlData(MediaCtlConfig &mediaCtlConfig);
    status_t getSensorMediaCtlData(MediaCtlConfig &mediaCtlConfig);
    void dumpMediaCtlConfig(const MediaCtlConfig &config) const;

    // Private helpers for port nodes
    status_t portGetFourCCInfo(Node &portNode,
                               uint32_t &stageId, uint32_t &terminalId);
    // Format options methods
    status_t getActiveOutputPorts(
            const StreamToSinkMap &streamToSinkIdMap);
    Node *getOutputPortForSink(const std::string &sinkName);
    status_t getSinkFormatOptions();
    bool isVideoRecordPort(Node *sink);
    // Kernel list generation methods
    void deleteKernelInfo();
    void createKernelListStructures();
    status_t generateKernelListsForStreams();

private:
    // Disable copy constructor and assignment operator
    GraphConfig(const GraphConfig &);
    GraphConfig& operator=(const GraphConfig &);
    void isNeedPathCrop(uint32_t path_input_w,
                        uint32_t path_input_h,
                        bool sp_enabled,
                        std::vector<camera3_stream_t*>& outputStream,
                        bool& mp_need_crop,
                        bool& sp_need_crop);
    void limitPathOutputSize(uint32_t& path_out_w, uint32_t& path_out_h);

private:
    GraphConfigManager *mManager; /* GraphConfig doesn't own mManager */
    GCSS::GraphConfigNode *mSettings;
    int32_t mReqId;
    StreamsMap mStreamIds;
    std::map<int32_t, size_t> mKernelCountsMap; // key is stream id

    bool mMetaEnabled; // indicates if the specific sensor provides sensor
                       // embedded metadata
    bool mFallback;
    PipeType mPipeType;
    enum SourceType {
        SRC_NONE = 0,
        SRC_SENSOR,
        SRC_TPG,
    };
    SourceType mSourceType;
    std::string mSourcePortName; // Sensor or TPG port name

    /**
     * pre-computed state done *per request*.
     * This map holds the terminal id's of the ISA's peer ports (this is
     * the terminal id's of the input port of the video or still pipe)
     * that are required to fulfill a request.
     * Ideally this gets initialized during init() call.
     * But for now the GcManager will set it via a private method.
     * we use a map so that we can handle the case when a request has 2 buffers
     * that are generated from the same pipe.
     */
    std::map<uid_t, uid_t> mIsaActiveDestinations;
    std::set<int32_t> mActiveStreamId;
    /**
     * vector holding one structure per virtual sink that stores the stream id
     * (pipeline id) associated with it and the terminal id of the input port
     * of that stream.
     * This vector is updated once per stream config.
     */
    std::vector<SinkDependency> mSinkDependencies;
    /**
     * vector holding the peers to the sink nodes. Map contains pairs of
     * {sink, peer}.
     * This map is filled at stream config time.
     */
    std::map<Node*, Node*> mSinkPeerPort;
    /**
     *copy of the map provided from GraphConfigManager to be used internally.
     */
    StreamToSinkMap mStreamToSinkIdMap;
    std::map<std::string, int32_t> mIsaOutputPort2StreamId;
    /**
     * Map of tuning modes per stream id
     * Key: stream id
     * Value: tuning mode
     */
    std::map<int32_t, int32_t> mStream2TuningMap;

    std::string mCSIBE;
    std::shared_ptr<MediaController> mMediaCtl;

    string mMainNodeName;
    string mSecondNodeName;
    bool   mIsMipiInterface;
    bool   mSensorLinkedToCIF;
    string mSnsLinkedPhyEntNm;
    bool   mMpOutputRaw;
    SensorFormat mAvailableSensorFormat;
    MediaCtlFormatParams mCurSensorFormat;
};

} // namespace camera2
} // namespace android
#endif
