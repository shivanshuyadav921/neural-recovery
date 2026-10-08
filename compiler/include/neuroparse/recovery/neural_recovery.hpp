#pragma once

#include "neuroparse/lexer/token.hpp"
#include "neuroparse/parser/parser_error.hpp"
#include "neuroparse/recovery/context_extractor.hpp"
#include "neuroparse/recovery/error_context.hpp"
#include "neuroparse/recovery/recovery_result.hpp"
#include "neuroparse/recovery/recovery_strategy.hpp"
#include "neuroparse/recovery/repair_applier.hpp"
#include "neuroparse/recovery/repair_candidate.hpp"
#include "neuroparse/recovery/repair_validator.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace neuroparse::recovery {

class NeuralInferenceEngine {
public:
  virtual ~NeuralInferenceEngine() = default;
  [[nodiscard]] virtual bool is_ready() const noexcept = 0;
  [[nodiscard]] virtual std::optional<RepairPrediction> predict(const ErrorContext& context) = 0;
  [[nodiscard]] virtual std::string_view engine_name() const noexcept = 0;
};

// Built-in rule-assisted candidate generator
class RuleAssistedInferenceEngine : public NeuralInferenceEngine {
public:
  [[nodiscard]] bool is_ready() const noexcept override { return true; }
  [[nodiscard]] std::optional<RepairPrediction> predict(const ErrorContext& context) override;
  [[nodiscard]] std::string_view engine_name() const noexcept override { return "RuleAssistedEngine"; }
};

// Fixture engine for testing deterministic acceptance/rejection flows
class MockInferenceEngine : public NeuralInferenceEngine {
public:
  explicit MockInferenceEngine(std::optional<RepairPrediction> fixed_prediction = std::nullopt)
      : ready_(true), fixed_prediction_(std::move(fixed_prediction)) {}

  void set_prediction(std::optional<RepairPrediction> pred) { fixed_prediction_ = std::move(pred); }
  void set_ready(bool ready) { ready_ = ready; }

  [[nodiscard]] bool is_ready() const noexcept override { return ready_; }
  [[nodiscard]] std::optional<RepairPrediction> predict(const ErrorContext& /*context*/) override {
    return fixed_prediction_;
  }
  [[nodiscard]] std::string_view engine_name() const noexcept override { return "MockEngine"; }

private:
  bool ready_ = true;
  std::optional<RepairPrediction> fixed_prediction_;
};

class NeuralRecoveryStrategy : public ErrorRecoveryStrategy {
public:
  NeuralRecoveryStrategy();
  explicit NeuralRecoveryStrategy(std::shared_ptr<NeuralInferenceEngine> engine);
  NeuralRecoveryStrategy(
      std::shared_ptr<NeuralInferenceEngine> engine,
      RepairValidator validator,
      RepairApplier applier);

  [[nodiscard]] RecoveryStrategyType strategy_type() const noexcept override {
    return RecoveryStrategyType::Neural;
  }
  [[nodiscard]] std::string_view name() const noexcept override {
    return "neural";
  }

  void set_inference_engine(std::shared_ptr<NeuralInferenceEngine> engine) {
    engine_ = std::move(engine);
  }
  [[nodiscard]] const NeuralInferenceEngine* inference_engine() const noexcept {
    return engine_.get();
  }

  [[nodiscard]] const RepairValidator& validator() const noexcept { return validator_; }
  [[nodiscard]] RepairValidator& validator() noexcept { return validator_; }

  [[nodiscard]] const RepairApplier& applier() const noexcept { return applier_; }

  RecoveryResult recover(
      std::vector<lexer::Token>& tokens,
      std::size_t& cursor,
      const parser::ParserError& error,
      std::string_view construct_context) override;

private:
  ContextExtractor extractor_;
  std::shared_ptr<NeuralInferenceEngine> engine_;
  RepairValidator validator_;
  RepairApplier applier_;
};

}  // namespace neuroparse::recovery
