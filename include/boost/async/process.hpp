//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASYNC_PROCESS_HPP
#define BOOST_ASYNC_PROCESS_HPP

#include <memory>
#include <boost/optional.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/start_dir.hpp>
#include <boost/process/v2/stdio.hpp>
#include <boost/process/v2/environment.hpp>

namespace boost::async
{

struct process_args
{
  boost::process::v2::process_stdio stdio;
  boost::process::v2::process_start_dir start_dir{boost::process::v2::filesystem::current_path()};
  boost::process::v2::process_environment env    {boost::process::v2::environment::current()};
};


/// A class managing a subprocess
/** A `process` object manages a subprocess; it tracks the status and exit-code,
 * and will terminate the process on destruction if `detach` was not called.
*/
struct process
{
  /// The underlying asio implementation type.
  typedef boost::process::v2::basic_process<asio::io_context::executor_type> implementation_type;

  /// The executor of the process
  using executor_type = asio::io_context::executor_type;
  /// Get the executor of the process
  executor_type get_executor();

  /// The non-closing handle type
  using handle_type = boost::process::v2::basic_process_handle<executor_type>;

  /// Get the underlying non-closing handle
  handle_type & handle();

  /// Get the underlying non-closing handle
  const handle_type & handle() const;

  /// Provides access to underlying operating system facilities
  using native_handle_type = typename handle_type::native_handle_type;

  /** An empty process is similar to a default constructed thread. It holds an empty
  handle and is a place holder for a process that is to be launched later. */
  process(implementation_type && impl);

  process(const process&) = delete;
  process& operator=(const process&) = delete;

  /// Move construct the process. It will be detached from `lhs`.
  process(process&& lhs);

  /// Move assign a process. It will be detached from `lhs`.
  process& operator=(process&& lhs);
  

  /// Construct a child from a property list and launch it using the default launcher..
  explicit process(
      executor_type executor,
      const filesystem::path& exe,
      std::initializer_list<string_view> args,
      process_args && arg = {});
  
  /// Construct a child from a property list and launch it using the default launcher..
  explicit process(
      executor_type executor,
      const filesystem::path& exe,
      std::initializer_list<wstring_view> args,
      process_args && arg = {});

  /// Construct a child from a property list and launch it using the default launcher..
  explicit process(
      executor_type executor,
      const filesystem::path& exe,
      std::vector<string_view> args,
      process_args && arg = {});

  /// Construct a child from a property list and launch it using the default launcher..
  explicit process(
      asio::io_context & context,
      const filesystem::path& exe,
      std::initializer_list<string_view> args,
      process_args && arg = {});
  
  /// Construct a child from a property list and launch it using the default launcher.
  explicit process(
      asio::io_context & context,
      const filesystem::path& exe,
      std::vector<string_view> args,
      process_args && arg = {});

  /// Attach to an existing process
  explicit process(executor_type exec, boost::process::v2::pid_type pid);

  /// Attach to an existing process and the internal handle
  explicit process(executor_type exec, boost::process::v2::pid_type pid, native_handle_type native_handle);

  /// Create an invalid handle
  explicit process(executor_type exec);

  /// Attach to an existing process
  explicit process(asio::io_context & context, boost::process::v2::pid_type pid);
  /// Attach to an existing process and the internal handle
  explicit process(asio::io_context & context, boost::process::v2::pid_type pid, native_handle_type native_handle);

  /// Create an invalid handle
  explicit process(asio::io_context & context);


  /// Destruct the handle and terminate the process if it wasn't detached.
  ~process();
  /// Sends the process a signal to ask for an interrupt, which the process may interpret as a shutdown.
  /** Maybe be ignored by the subprocess. */

  void interrupt();
  /// Throwing @overload void interrupt()
  void interrupt(system::error_code & ec);

  /// Throwing @overload void request_exit(error_code & ec)
  void request_exit();
  /// Sends the process a signal to ask for a graceful shutdown. Maybe be ignored by the subprocess.
  void request_exit(system::error_code & ec);

  /// Throwing @overload void terminate(native_exit_code_type &exit_code, error_code & ec)
  void terminate();
  /// Unconditionally terminates the process and stores the exit code in exit_status.
  void terminate(system::error_code & ec);

 private:
  struct wait_op_
  {
    process * proc;

    std::optional<std::tuple<system::error_code, boost::process::v2::native_exit_code_type>> result;
    std::exception_ptr error;
    constexpr static bool await_ready() {return false;}

    void async_accept(boost::async::completion_handler<system::error_code> h);

    template<typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> h)
    {
      try
      {
        proc->async_wait({h, result});
        return true;
      }
      catch(...)
      {
        error = std::current_exception();
        return false;
      }
    }

    void await_resume()
    {
      if (error)
        std::rethrow_exception(std::exchange(error, nullptr));

      if (std::get<0>(*result))
        throw system::system_error(std::get<0>(*result));
    }
  };

  struct wait_op_ec_
  {
    process * proc;
    system::error_code & ec;

    std::optional<std::tuple<system::error_code, boost::process::v2::native_exit_code_type>> result;
    std::exception_ptr error;
    constexpr static bool await_ready() {return false;}

    void async_accept(boost::async::completion_handler<system::error_code> h);

    template<typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> h)
    {
      try
      {
        proc->async_wait({h, result});
        return true;
      }
      catch(...)
      {
        error = std::current_exception();
        return false;
      }
    }

    void await_resume()
    {
      if (error)
        std::rethrow_exception(std::exchange(error, nullptr));

      if (std::get<0>(*result))
        ec = std::get<0>(*result);
    }
  };
 public:


  /// Throwing @overload wait(error_code & ec)
  int wait();
  /// Waits for the process to exit, store the exit code internall and return it.
  int wait(system::error_code & ec);

  /// Detach the process.
  handle_type detach();

  int exit_code() const;

  /// Get the id of the process;
  boost::process::v2::pid_type id() const;

  /// The native handle of the process.
  /** This might be undefined on posix systems that only support signals */
  boost::process::v2::native_exit_code_type native_exit_code() const;
  /// Checks if the current process is running.
  /** If it has already completed the exit code will be stored internally
   * and can be obtained by calling `exit_code.
   */
  bool running();

  /// Throwing @overload bool running(error_code & ec)
  bool running(system::error_code & ec) noexcept;

  /// Check if the process is referring to an existing process.
  /** Note that this might be a process that already exited.*/
  bool is_open() const;

  /// Asynchronously wait for the process to exit and deliver the portable exit-code in the completion handler.
  void async_wait(completion_handler<system::error_code, boost::process::v2::native_exit_code_type> h);


  /// Get the underlying asio implementation.
  implementation_type & implementation() {return impl_;}

 private:
  implementation_type impl_;

};


}

#endif //BOOST_ASYNC_PROCESS_HPP
