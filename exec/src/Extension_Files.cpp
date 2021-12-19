#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "category-source.hpp"
#include "Streamlined_Files.hpp"
#include "Category_Bool.hpp"
#include "Category_FileDescriptor.hpp"
#include "Category_Files.hpp"

#ifdef ZEOLITE_PUBLIC_NAMESPACE
namespace ZEOLITE_PUBLIC_NAMESPACE {
#endif  // ZEOLITE_PUBLIC_NAMESPACE

struct ExtCategory_Files : public Category_Files {
};

struct ExtType_Files : public Type_Files {
  inline ExtType_Files(Category_Files& p, Params<0>::Type params) : Type_Files(p, params) {}

  ReturnTuple Call_blocking(const ParamsArgs& params_args) const final {
    TRACE_FUNCTION("Files.blocking")
    const BoxedValue& descriptor = params_args.GetArg(0);
    const PrimBool blocking = (params_args.GetArg(1)).AsBool();
    const PrimInt fd = TypeValue::Call(descriptor, Function_FileDescriptor_get, PassParamsArgs()).At(0).AsInt();
    const int old = fcntl(fd, F_GETFL);
    if (old < 0) {
      FAIL() << "Failed to get flags for descriptor " << fd << ": " << strerror(errno);
    }
    int status = 0;
    if (blocking) {
      status = fcntl(fd, F_SETFL & ~O_NONBLOCK);
    } else {
      status = fcntl(fd, F_SETFL | O_NONBLOCK);
    }
    if (status < 0) {
      FAIL() << "Failed to set flags for descriptor " << fd << ": " << strerror(errno);
    }
    return ReturnTuple(descriptor);
  }

  ReturnTuple Call_close(const ParamsArgs& params_args) const final {
    TRACE_FUNCTION("Files.close")
    const BoxedValue& descriptor = params_args.GetArg(0);
    const PrimInt fd = TypeValue::Call(descriptor, Function_FileDescriptor_get, PassParamsArgs()).At(0).AsInt();
    close(fd);
    return ReturnTuple();
  }

  ReturnTuple Call_closeExec(const ParamsArgs& params_args) const final {
    TRACE_FUNCTION("Files.closeExec")
    const BoxedValue& descriptor = params_args.GetArg(0);
    const PrimBool cloexec = (params_args.GetArg(1)).AsBool();
    const PrimInt fd = TypeValue::Call(descriptor, Function_FileDescriptor_get, PassParamsArgs()).At(0).AsInt();
    const int old = fcntl(fd, F_GETFD);
    if (old < 0) {
      FAIL() << "Failed to get flags for descriptor " << fd << ": " << strerror(errno);
    }
    int status = 0;
    if (cloexec) {
      status = fcntl(fd, F_SETFD | FD_CLOEXEC);
    } else {
      status = fcntl(fd, F_SETFD & ~FD_CLOEXEC);
    }
    if (status < 0) {
      FAIL() << "Failed to set flags for descriptor " << fd << ": " << strerror(errno);
    }
    return ReturnTuple(descriptor);
  }

  ReturnTuple Call_pipe(const ParamsArgs& params_args) const final {
    TRACE_FUNCTION("Files.pipe")
    int pipes[2];
    if (pipe(pipes) < 0) {
      FAIL() << "Failed to create pipe pair: " << strerror(errno);
    }
    const BoxedValue pipe0 = TypeInstance::Call(GetType_FileDescriptor(Params<0>::Type()),
                                                Function_FileDescriptor_new,
                                                PassParamsArgs(Box_Int(pipes[0]))).At(0);
    const BoxedValue pipe1 = TypeInstance::Call(GetType_FileDescriptor(Params<0>::Type()),
                                                Function_FileDescriptor_new,
                                                PassParamsArgs(Box_Int(pipes[1]))).At(0);
    return ReturnTuple(pipe0, pipe1);
  }
};

Category_Files& CreateCategory_Files() {
  static auto& category = *new ExtCategory_Files();
  return category;
}

S<const Type_Files> CreateType_Files(const Params<0>::Type& params) {
  static const auto cached = S_get(new ExtType_Files(CreateCategory_Files(), Params<0>::Type()));
  return cached;
}

void RemoveType_Files(const Params<0>::Type& params) {}

#ifdef ZEOLITE_PUBLIC_NAMESPACE
}  // namespace ZEOLITE_PUBLIC_NAMESPACE
using namespace ZEOLITE_PUBLIC_NAMESPACE;
#endif  // ZEOLITE_PUBLIC_NAMESPACE
