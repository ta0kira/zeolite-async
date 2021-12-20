#include <sstream>
#include <vector>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "category-source.hpp"
#include "Streamlined_Command.hpp"
#include "Category_AsyncTask.hpp"
#include "Category_AsyncValue.hpp"
#include "Category_Bool.hpp"
#include "Category_Command.hpp"
#include "Category_Container.hpp"
#include "Category_ErrorOr.hpp"
#include "Category_FileDescriptor.hpp"
#include "Category_Int.hpp"
#include "Category_ReadAt.hpp"
#include "Category_String.hpp"

#ifdef ZEOLITE_PUBLIC_NAMESPACE
namespace ZEOLITE_PUBLIC_NAMESPACE {
#endif  // ZEOLITE_PUBLIC_NAMESPACE

BoxedValue CreateValue_Command(S<const Type_Command> parent, const ParamsArgs& params_args);

struct ExtCategory_Command : public Category_Command {
};

struct ExtType_Command : public Type_Command {
  inline ExtType_Command(Category_Command& p, Params<0>::Type params) : Type_Command(p, params) {}

  ReturnTuple Call_new(const ParamsArgs& params_args) const final {
    TRACE_FUNCTION("Command.new")
    return ReturnTuple(CreateValue_Command(PARAM_SELF, params_args));
  }
};

struct ExtValue_Command : public Value_Command {
  inline ExtValue_Command(S<const Type_Command> p, const ParamsArgs& params_args)
    : Value_Command(std::move(p)),
      // Setting command_[0] allows GetFd to set error_ with the command name if necessary.
      command_{params_args.GetArg(0).AsString()},
      stdin_(GetFd(params_args.GetArg(2))),
      stdout_(GetFd(params_args.GetArg(3))),
      stderr_(GetFd(params_args.GetArg(4))) {
    const BoxedValue& args = params_args.GetArg(1);
    const PrimInt count = TypeValue::Call(args, Function_Container_size, PassParamsArgs()).At(0).AsInt();
    for (int i = 0; i < count; ++i) {
      command_.push_back(TypeValue::Call(args, Function_ReadAt_readAt, PassParamsArgs(Box_Int(i))).At(0).AsString());
    }
  }

  ~ExtValue_Command() {
    if (executed_ && process_ > 0 && !finished_) {
      kill(process_, SIGTERM);
    }
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

  ReturnTuple Call_start(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("Command.start")
    Start(false);
    return ReturnTuple(VAR_SELF);
  }

  ReturnTuple Call_tryFinish(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("Command.tryFinish")
    return ReturnTuple(Box_Bool(Finish(false)));
  }

  ReturnTuple Call_runDetached(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("Command.runDetached")
    Start(true);
    return ReturnTuple(Call_get(PassParamsArgs()));
  }

  ReturnTuple Call_runOnce(const ParamsArgs& params_args) final {
    TRACE_FUNCTION("Command.runOnce")
    Call_start(PassParamsArgs());
    Call_finish(PassParamsArgs());
    return ReturnTuple(Call_get(PassParamsArgs()));
  }

  void Start(bool detached) {
    if (executed_) {
      return;
    }
    process_ = fork();
    if (process_ == 0) {
      // This makes sure that an immediate call to ~ExtValue_Command() in the
      // parent doesn't create a stack trace in the child.
      signal(SIGTERM, SIG_DFL);
      if (detached) {
        // This makes sure that killing the parent's process group doesn't kill
        // this process. Otherwise, runDetached() wouldn't work as expected.
        setsid();
      }
      SetFd(stdin_,  STDIN_FILENO,  "stdin");
      SetFd(stdout_, STDOUT_FILENO, "stdout");
      SetFd(stderr_, STDERR_FILENO, "stderr");
      std::unique_ptr<char*[]> argv(new char*[command_.size()+1]);
      for (int i = 0; i < command_.size(); ++i) {
        argv[i] = const_cast<char*>(command_[i].c_str());
      }
      argv[command_.size()] = nullptr;
      raise(SIGSTOP);
      execvp(argv[0], argv.get());
      std::cerr << "Error executing " << command_[0] << ": " << strerror(errno) << std::endl;
      _exit(1);
    } else {
      CloseFd(stdin_);
      CloseFd(stdout_);
      CloseFd(stderr_);
      executed_ = true;
    }
    if (process_ < 0) {
      SetError(strerror(errno));
      finished_ = true;
    } else {
      int status = 0;
      int result = 0;
      // Wait for the process to stop right before execvp. This is to ensure
      // that signal handlers, session, etc. are configured in the child before
      // the parent has a chance to kill the child.
      while ((result = waitpid(process_, &status, WUNTRACED)) == 0 && !WIFEXITED(status) && !WIFSTOPPED(status));
      kill(process_, SIGCONT);
      // Wait for the process to continue.
      while ((result = waitpid(process_, &status, WCONTINUED)) == 0 && !WIFEXITED(status) && !WIFCONTINUED(status));
      if (detached) {
        finished_ = true;
        status_ = 0;
      }
      if (result < 1) {
        // The only exit calls before execvp are related to file descriptors.
        finished_ = true;
        SetError("Error setting file descriptors");
      }
    }
  }

  void SetError(const std::string& message) {
    std::ostringstream error;
    error << "Error executing " << command_[0] << ": " << message;
    error_ = error.str();
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
    while ((result = waitpid(process_, &status, block ? 0 : WNOHANG)) == 0 && !WIFEXITED(status)) {
      if (!block) break;
    }
    if (result > 0) {
      // Exited.
      finished_ = true;
      status_ = WEXITSTATUS(status);
      return true;
    } else if (result < 0) {
      // Error with wait call.
      SetError(strerror(errno));
      finished_ = true;
      return true;
    } else {
      // Not exited yet.
      return false;
    }
  }

  static void SetFd(int source, int dest, const std::string& name) {
    if (source >= 0) {
      if (dup2(source, dest) < 0) {
        std::cerr << "Failed to set " << name << ": " << strerror(errno) << std::endl;
        _exit(1);
      }
    }
  }

  static void CloseFd(int fd) {
    if (fd > 2) {
      close(fd);
    }
  }

  int GetFd(const BoxedValue& descriptor) {
    int fd = -1;
    if (!finished_ && BoxedValue::Present(descriptor)) {
      fd = TypeValue::Call(BoxedValue::Require(descriptor),
                           Function_FileDescriptor_get,
                           PassParamsArgs()).At(0).AsInt();
      if (fd >= 0 && fd <= 2) {
        // This prevents latent issues if stdout or stderr is redirected to the
        // other, and then also replaced.
        fd = dup(fd);
        if (fd < 0) {
          // This might happen if the parent process is out of file descriptors.
          executed_ = true;
          finished_ = true;
          SetError(strerror(errno));
        }
      }
      if (fd > 2) {
        fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
      }
    }
    return fd;
  }

  bool executed_ = false;
  bool finished_ = false;
  int status_ = 0;
  std::string error_;
  pid_t process_ = 0;
  std::vector<PrimString> command_;
  const int stdin_;
  const int stdout_;
  const int stderr_;
};

Category_Command& CreateCategory_Command() {
  static auto& category = *new ExtCategory_Command();
  return category;
}

S<const Type_Command> CreateType_Command(const Params<0>::Type& params) {
  static const auto cached = S_get(new ExtType_Command(CreateCategory_Command(), Params<0>::Type()));
  return cached;
}

void RemoveType_Command(const Params<0>::Type& params) {}

BoxedValue CreateValue_Command(S<const Type_Command> parent, const ParamsArgs& params_args) {
  return BoxedValue::New<ExtValue_Command>(std::move(parent), params_args);
}

#ifdef ZEOLITE_PUBLIC_NAMESPACE
}  // namespace ZEOLITE_PUBLIC_NAMESPACE
using namespace ZEOLITE_PUBLIC_NAMESPACE;
#endif  // ZEOLITE_PUBLIC_NAMESPACE
