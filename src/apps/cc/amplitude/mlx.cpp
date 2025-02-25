#include "mlx.h"

#include <seiscomp/datamodel/comment.h>

#include <algorithm>
#include <boost/algorithm/string/join.hpp>

#include "../settings.h"
#include "../util/memory.h"
#include "../util/waveform_stream_id.h"
#include "util.h"

namespace Seiscomp {
namespace detect {
namespace amplitude {

const MLx::CombiningStrategy MLx::max =
    [](const std::vector<detect::AmplitudeProcessor::AmplitudeCPtr> &amplitudes,
       detect::AmplitudeProcessor::Amplitude &combined) {
      assert(!amplitudes.empty());
      auto result{std::max_element(
          std::begin(amplitudes), std::end(amplitudes),
          [](const AmplitudeCPtr &lhs, const AmplitudeCPtr &rhs) {
            return lhs->value.value < rhs->value.value;
          })};

      combined = *(*result);
    };

MLx::MLx(
    std::vector<CombiningAmplitudeProcessor::AmplitudeProcessor> underlying)
    : CombiningAmplitudeProcessor{std::move(underlying), MLx::max} {
  assert((detect::util::isUniqueSensorLocation(associatedWaveformStreamIds())));
  setType("MLx");
  setUnit("M/S");
}

void MLx::finalize(DataModel::Amplitude *amplitude) const {
  CombiningAmplitudeProcessor::finalize(amplitude);

  amplitude->setType(type());
  amplitude->setUnit(unit());

  util::setWaveformStreamId(this, *amplitude);

  try {
    auto comment{util::createDetectorIdComment(this)};
    amplitude->add(comment.release());
  } catch (const Exception &) {
  }

  std::vector<std::string> pickPublicIds;
  for (const auto &proc : *this) {
    const auto &env{proc.environment()};
    for (const auto &pick : env.picks) {
      pickPublicIds.emplace_back(pick->publicID());
    }
  }
  // pick public identifiers
  {
    auto comment{detect::util::make_smart<DataModel::Comment>()};
    comment->setId(settings::kAmplitudePicksCommentId);
    comment->setText(
        boost::algorithm::join(pickPublicIds, settings::kPublicIdSep));
    amplitude->add(comment.get());
  }

  {
    auto comment{util::createAssociatedWaveformStreamIdComment(this)};
    amplitude->add(comment.release());
  }
}

}  // namespace amplitude
}  // namespace detect
}  // namespace Seiscomp
