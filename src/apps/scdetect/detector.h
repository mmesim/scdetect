#ifndef SCDETECT_APPS_SCDETECT_DETECTOR_H_
#define SCDETECT_APPS_SCDETECT_DETECTOR_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <seiscomp/core/datetime.h>
#include <seiscomp/core/recordsequence.h>
#include <seiscomp/core/timewindow.h>
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/magnitude.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/datamodel/sensorlocation.h>

#include <boost/optional.hpp>

#include "builder.h"
#include "config.h"
#include "detector/detector.h"
#include "detector/pot.h"
#include "processor.h"
#include "settings.h"
#include "template.h"
#include "waveform.h"

namespace Seiscomp {
namespace detect {

class DetectorBuilder;

DEFINE_SMARTPOINTER(Detector);
class Detector : public Processor {

  Detector(const std::string &id, const DataModel::OriginCPtr &origin);

public:
  DEFINE_SMARTPOINTER(Detection);
  struct Detection : public Result {
    double fit{};

    Core::Time time{};
    double latitude{};
    double longitude{};
    double depth{};

    double magnitude{};

    size_t num_stations_associated{};
    size_t num_stations_used{};
    size_t num_channels_associated{};
    size_t num_channels_used{};

    using TemplateResult = detector::Detector::Result::TemplateResult;
    using TemplateResults =
        std::unordered_multimap<std::string, TemplateResult>;
    // Template specific results
    TemplateResults template_results;
  };

  friend class DetectorBuilder;
  static DetectorBuilder Create(const std::string &detector_id,
                                const std::string &origin_id);

  void set_filter(Filter *filter) override;

  // Sets the maximal gap length to be tolerated
  void set_gap_tolerance(const Core::TimeSpan &duration);
  // Returns the gap tolerance
  const Core::TimeSpan gap_tolerance() const;
  // Enables/disables the linear interpolation of missing samples
  // if the gap is smaller than the configured gap tolerance
  void set_gap_interpolation(bool e);
  // Returns if gap interpolation is enabled or disabled, respectively
  bool gap_interpolation() const;

  bool Feed(const Record *rec) override;
  void Reset() override;

  std::string DebugString() const override;

  bool WithPicks() const override;

protected:
  void Process(StreamState &stream_state, RecordCPtr record,
               const DoubleArray &filtered_data) override;

  bool HandleGap(StreamState &stream_state, RecordCPtr record,
                 DoubleArrayPtr data) override;

  void Fill(StreamState &stream_state, RecordCPtr record, size_t n,
            double *samples) override;

  void InitStream(StreamState &stream_state, RecordCPtr record) override;

  bool EnoughDataReceived(const StreamState &stream_state) const override;
  // Callback function storing `res`
  void StoreDetection(const detector::Detector::Result &res);
  // Prepares the detection from `res`
  void PrepareDetection(DetectionPtr &d, const detector::Detector::Result &res);

private:
  // Fill gaps
  bool FillGap(StreamState &stream_state, RecordCPtr record,
               const Core::TimeSpan &duration, double next_sample,
               size_t missing_samples);

  struct StreamConfig;
  using WaveformStreamID = std::string;
  using StreamConfigs = std::unordered_map<WaveformStreamID, StreamConfig>;

  struct StreamConfig {
    Processor::StreamState stream_state;
    // TODO(damb): Is a stream_config required at this level?
    // - The detector does no filtering -> no gain is applied.
    // -
    // Processing::Stream stream_config;
    std::shared_ptr<RecordSequence> stream_buffer;
  };

  StreamConfigs stream_configs_;

  DetectorConfig config_;

  detector::Detector detector_;
  boost::optional<detector::Detector::Result> detection_;

  DataModel::OriginCPtr origin_;
  DataModel::EventPtr event_;
  DataModel::MagnitudePtr magnitude_;

  std::multimap<WaveformStreamID, Template::MatchResultCPtr> debug_cc_results_;
};

class DetectorBuilder : public Builder<Detector> {

public:
  DetectorBuilder(const std::string &detector_id, const std::string &origin_id);

  DetectorBuilder &set_config(const DetectorConfig &config);

  DetectorBuilder &set_eventparameters();
  // Set stream related template configuration
  DetectorBuilder &
  set_stream(const std::string &stream_id, const StreamConfig &stream_config,
             WaveformHandlerIfacePtr wf_handler,
             const boost::filesystem::path &path_debug_info = "");
  // Set the path to the debug info directory
  DetectorBuilder &set_debug_info_dir(const boost::filesystem::path &path);

protected:
  void Finalize() override;

  bool IsValidArrival(const DataModel::ArrivalCPtr arrival,
                      const DataModel::PickCPtr pick);

private:
  struct TemplateProcessorConfig {
    // Template matching processor
    std::unique_ptr<Processor> processor;

    struct MetaData {
      // TODO(damb): Sensor location is not required, anymore
      DataModel::SensorLocationCPtr sensor_location;
      // The template related pick
      DataModel::PickCPtr pick;
      // The template related arrival
      DataModel::ArrivalCPtr arrival;
      // The template waveform pick offset
      Core::TimeSpan pick_offset;
    } metadata;
  };

  std::string origin_id_;

  std::vector<detector::POT::ArrivalPick> arrival_picks_;

  using TemplateProcessorConfigs =
      std::unordered_map<std::string, TemplateProcessorConfig>;
  TemplateProcessorConfigs processor_configs_;
};

} // namespace detect
} // namespace Seiscomp

#endif // SCDETECT_APPS_SCDETECT_DETECTOR_H_
