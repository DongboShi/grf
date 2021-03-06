/*-------------------------------------------------------------------------------
  This file is part of generalized random forest (grf).

  grf is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  grf is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with grf. If not, see <http://www.gnu.org/licenses/>.
 #-------------------------------------------------------------------------------*/

#include <cmath>
#include "prediction/ProbabilityPredictionStrategy.h"

namespace grf {

ProbabilityPredictionStrategy::ProbabilityPredictionStrategy(size_t num_classes):
    num_classes(num_classes) {
};

size_t ProbabilityPredictionStrategy::prediction_length() const {
    return num_classes;
}

std::vector<double> ProbabilityPredictionStrategy::predict(const std::vector<double>& average) const {
  return average;
}

std::vector<double> ProbabilityPredictionStrategy::compute_variance(
    const std::vector<double>& average,
    const PredictionValues& leaf_values,
    size_t ci_group_size) const {
  std::vector<double> variance_estimates(num_classes);
  for (size_t cls = 0; cls < num_classes; ++cls) {
    double average_outcome = average.at(cls);

    double num_good_groups = 0;
    double psi_squared = 0;
    double psi_grouped_squared = 0;

    for (size_t group = 0; group < leaf_values.get_num_nodes() / ci_group_size; ++group) {
      bool good_group = true;
      for (size_t j = 0; j < ci_group_size; ++j) {
        if (leaf_values.empty(group * ci_group_size + j)) {
          good_group = false;
        }
      }
      if (!good_group) continue;

      num_good_groups++;

      double group_psi = 0;

      for (size_t j = 0; j < ci_group_size; ++j) {
        size_t i = group * ci_group_size + j;
        double psi_1 = leaf_values.get(i, cls) - average_outcome;

        psi_squared += psi_1 * psi_1;
        group_psi += psi_1;
      }

      group_psi /= ci_group_size;
      psi_grouped_squared += group_psi * group_psi;
    }

    double var_between = psi_grouped_squared / num_good_groups;
    double var_total = psi_squared / (num_good_groups * ci_group_size);

    // This is the amount by which var_between is inflated due to using small groups
    double group_noise = (var_total - var_between) / (ci_group_size - 1);

    // A simple variance correction, would be to use:
    // var_debiased = var_between - group_noise.
    // However, this may be biased in small samples; we do an objective
    // Bayes analysis of variance instead to avoid negative values.
    double var_debiased = bayes_debiaser.debias(var_between, group_noise, num_good_groups);
    variance_estimates[cls] = var_debiased;
  }

  return variance_estimates;
}

size_t ProbabilityPredictionStrategy::prediction_value_length() const {
  return num_classes;
}

PredictionValues ProbabilityPredictionStrategy::precompute_prediction_values(
    const std::vector<std::vector<size_t>>& leaf_samples,
    const Data& data) const {
  size_t num_leaves = leaf_samples.size();
  std::vector<std::vector<double>> values(num_leaves);

  for (size_t i = 0; i < num_leaves; i++) {
    const std::vector<size_t>& leaf_node = leaf_samples.at(i);
    if (leaf_node.empty()) {
      continue;
    }

    std::vector<double>& averages = values[i];
    averages.resize(num_classes);
    double weight_sum = 0.0;
    for (auto& sample : leaf_node) {
      size_t sample_class = data.get_outcome(sample);
      averages[sample_class] += data.get_weight(sample);
      weight_sum += data.get_weight(sample);
    }

    // if total weight is very small, treat the leaf as empty
    if (std::abs(weight_sum) <= 1e-16) {
      continue;
    }

    for (size_t cls = 0; cls < num_classes; ++cls) {
      averages[cls] = averages[cls] / weight_sum;
    }
  }

  return PredictionValues(values, num_classes);
}

std::vector<std::pair<double, double>> ProbabilityPredictionStrategy::compute_error(
    size_t sample,
    const std::vector<double>& average,
    const PredictionValues& leaf_values,
    const Data& data) const {
  return { std::make_pair<double, double>(NAN, NAN) };
}

} // namespace grf
