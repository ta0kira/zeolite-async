#include <vector>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "category-source.hpp"
#include "Streamlined_AsyncExec.hpp"
#include "Category_AlwaysEmpty.hpp"
#include "Category_AsyncCollector.hpp"
#include "Category_AsyncExec.hpp"
#include "Category_AsyncId.hpp"
#include "Category_AsyncNode.hpp"
#include "Category_AsyncValue.hpp"
#include "Category_Bool.hpp"
#include "Category_Container.hpp"
#include "Category_Default.hpp"
#include "Category_ErrorOr.hpp"
#include "Category_Formatted.hpp"
#include "Category_Int.hpp"
#include "Category_ReadAt.hpp"

#ifdef ZEOLITE_PRIVATE_NAMESPACE
namespace ZEOLITE_PRIVATE_NAMESPACE {
#endif  // ZEOLITE_PRIVATE_NAMESPACE

BoxedValue CreateValue_AsyncExec(S<const Type_AsyncExec> parent, std::vector<PrimString> command);

struct ExtCategory_AsyncExec : public Category_AsyncExec {
};

struct ExtType_AsyncExec : public Type_AsyncExec {
  inline ExtType_AsyncExec(Category_AsyncExec& p, Params<0>::Type params) : Type_AsyncExec(p, params) {}

  ReturnTuple Call_withArgs(const ParamsArgs& params_args) const final {
    TRACE_FUNCTION("AsyncExec.withArgs")
    std::vector<PrimString> command;
    command.push_back(params_args.GetArg(0).AsString());
    const BoxedValue& args = params_args.GetArg(1);
    const PrimInt count = TypeValue::Call(args, Function_Container_size, PassParamsArgs()).At(0).AsInt();
    for (int i = 0; i < count; ++i) {
      const BoxedValue arg = TypeValue::Call(args, Function_ReadAt_readAt, PassParamsArgs(Box_Int(i))).At(0);
      command.push_back(TypeValue::Call(arg, Function_Formatted_formatted, PassParamsArgs()).At(0).AsString());
    }
    return ReturnTuple(CreateValue_AsyncExec(PARAM_SELF, std::move(command)));
  }
};

struct ExtValue_AsyncExec : public Value_AsyncExec {
  inline ExtValue_AsyncExec(S<const Type_AsyncExec> p, std::vector<PrimString> command)
    : Value_AsyncExec(std::move(p)),
      id_(TypeInstance::Call(GetType_AsyncId(Params<0>::Type()), Function_AsyncId_new, PassParamsArgs()).At(0)),
      command_(std::move(command)) {}

  ReturnTuple Call_collect(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("AsyncExec.collect")
    const BoxedValue& collector = params_args.GetArg(0);
    const BoxedValue empty = TypeInstance::Call(GetType_AlwaysEmpty(Params<0>::Type()), Function_Default_default, PassParamsArgs()).At(0);
    (void) TypeValue::Call(collector, Function_AsyncCollector_include, PassParamsArgs(VAR_SELF, empty));
    return ReturnTuple();
  }

  ReturnTuple Call_finish(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("AsyncExec.finish")
    Finish(true);
    return ReturnTuple(VAR_SELF);
  }

  ReturnTuple Call_get(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("AsyncExec.get")
    if (!executed_) {
      FAIL() << "Process was never started";
    }
    if (!finished_) {
      FAIL() << "Process has not completed";
    }
    if (error_.size() > 0) {
      // Return ErrorOr:error(error_).
      return ReturnTuple(GetCategory_ErrorOr().Call(
        Function_ErrorOr_error, PassParamsArgs(Box_String(error_))));
    } else {
      // Return ErrorOr:value(status_).
      return ReturnTuple(GetCategory_ErrorOr().Call(
        Function_ErrorOr_value, PassParamsArgs(GetType_Int(Params<0>::Type()), Box_Int(status_))));
    }
  }

  ReturnTuple Call_getId(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("AsyncExec.getId")
    return ReturnTuple(id_);
  }

  ReturnTuple Call_start(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("AsyncExec.start")
    if (executed_) {
      return ReturnTuple(VAR_SELF);
    }
    process_ = fork();
    if (process_ == 0) {
      std::unique_ptr<char*[]> argv(new char*[command_.size()+1]);
      for (int i = 0; i < command_.size(); ++i) {
        argv[i] = const_cast<char*>(command_[i].c_str());
      }
      argv[command_.size()] = nullptr;
      _exit(execvp(argv[0], argv.get()));
    } else {
      executed_ = true;
    }
    if (process_ < 0) {
      error_ = strerror(errno);
      finished_ = true;
    }
    return ReturnTuple(VAR_SELF);
  }

  ReturnTuple Call_tryFinish(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("AsyncExec.tryFinish")
    return ReturnTuple(Box_Bool(Finish(false)));
  }

  ReturnTuple Call_runOnce(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("AsyncExec.runOnce")
    Call_start(PassParamsArgs());
    Call_finish(PassParamsArgs());
    return ReturnTuple(Call_get(PassParamsArgs()));
  }

  bool Finish(bool block) {
    if (!executed_) {
      FAIL() << "Process was never started";
    }
    if (finished_) {
      return true;
    }
    int status = 0;
    int result = 0;
    while ((result = waitpid(process_, &status, block ? 0 : WNOHANG)) == 0 && WIFEXITED(status)) {
      if (!block) break;
    }
    if (result > 0) {
      // Exited.
      finished_ = true;
      status_ = WEXITSTATUS(status);
      return true;
    } else if (result < 0) {
      // Error with wait call.
      error_ = strerror(errno);
      finished_ = true;
      return true;
    } else {
      // Not exited yet.
      return false;
    }
  }

  bool executed_ = false;
  bool finished_ = false;
  int status_ = 0;
  std::string error_;
  pid_t process_ = 0;
  const BoxedValue id_;
  const std::vector<PrimString> command_;
};

Category_AsyncExec& CreateCategory_AsyncExec() {
  static auto& category = *new ExtCategory_AsyncExec();
  return category;
}

S<const Type_AsyncExec> CreateType_AsyncExec(const Params<0>::Type& params) {
  static const auto cached = S_get(new ExtType_AsyncExec(CreateCategory_AsyncExec(), Params<0>::Type()));
  return cached;
}

void RemoveType_AsyncExec(const Params<0>::Type& params) {}

BoxedValue CreateValue_AsyncExec(S<const Type_AsyncExec> parent, std::vector<PrimString> command) {
  return BoxedValue::New<ExtValue_AsyncExec>(std::move(parent), std::move(command));
}

#ifdef ZEOLITE_PRIVATE_NAMESPACE
}  // namespace ZEOLITE_PRIVATE_NAMESPACE
using namespace ZEOLITE_PRIVATE_NAMESPACE;
#endif  // ZEOLITE_PRIVATE_NAMESPACE
