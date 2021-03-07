#include "template.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

#include "log.h"
#include "seiscomp/core/timewindow.h"
#include "settings.h"
#include "utils.h"
#include "waveform.h"

namespace Seiscomp {
namespace detect {

Template::Template(const GenericRecordCPtr &template_wf, const std::string &id,
                   const Processor *p)
    : WaveformProcessor{p ? std::string{p->id() + settings::kProcessorIdSep +
                                        id}
                          : id},
      cross_correlation_{template_wf} {}

void Template::set_filter(Filter *filter) {
  if (stream_state_.filter)
    delete stream_state_.filter;

  stream_state_.filter = filter;
}

bool Template::Feed(const Record *record) {
  if (record->sampleCount() == 0)
    return false;

  return Store(stream_state_, record);
}

void Template::Reset() {
  Filter *tmp{stream_state_.filter};

  stream_state_ = StreamState{};
  if (tmp) {
    stream_state_.filter = tmp->clone();
    delete tmp;
  }

  cross_correlation_.Reset();

  WaveformProcessor::Reset();
}

void Template::Process(StreamState &stream_state, const Record *record,
                       const DoubleArray &filtered_data) {

  const auto n{static_cast<size_t>(filtered_data.size())};
  set_status(Status::kInProgress, 1);

  int start_idx{0};
  Core::Time start{record->timeWindow().startTime()};
  // check if processing start lies within the record
  if (!stream_state_.initialized) {
    start_idx =
        std::max(0, static_cast<int>(n) -
                        static_cast<int>(stream_state_.received_samples -
                                         stream_state_.needed_samples));
    double t = start_idx / n;
    start =
        record->startTime() + Core::TimeSpan{record->timeWindow().length() * t};
  }

  double coefficient{std::nan("")}, lag_idx{0};
  // determine the first maximum correlation coefficient
  for (size_t i{static_cast<size_t>(start_idx)}; i < n; ++i) {
    const double v{filtered_data[i]};
    if (!isfinite(coefficient) || coefficient < v) {
      coefficient = v;
      lag_idx = i;
    }
  }

  // take cross-correlation filter delay into account i.e. the template
  // processor's result is referring to a time window shifted to the past
  const auto t{
      static_cast<double>(lag_idx - cross_correlation_.template_size()) /
      (n + cross_correlation_.template_size())};
  const Core::TimeSpan template_length{cross_correlation_.template_length()};
  const Core::TimeWindow tw{start - template_length,
                            record->endTime() - template_length};

  auto result{utils::make_smart<MatchResult>(coefficient, tw.length() * t, tw)};
  EmitResult(record, result.get());
}

void Template::Fill(StreamState &stream_state, const Record *record,
                    DoubleArrayPtr &data) {

  // TODO(damb): Allow target sampling frequency to be configurable if filter
  // is in use.
  WaveformProcessor::Fill(stream_state, record, data);

  waveform::Demean(*data);
  cross_correlation_.Apply(data->size(), data->typedData());
}

void Template::InitStream(StreamState &stream_state, const Record *record) {
  WaveformProcessor::InitStream(stream_state, record);
  cross_correlation_.set_sampling_frequency(stream_state.sampling_frequency);
}

} // namespace detect
} // namespace Seiscomp
