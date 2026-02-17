#pragma once

#include <cerrno>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Utility for managing background child processes.
 * Encapsulates fork(), waitpid(), and status tracking.
 */
class ProcessUtil
{
public:
    enum class Status
    {
        RUNNING,
        EXITED,
        SIGNALED,
        ERROR,
        NONE
    };

    ProcessUtil() = default;

    /**
     * Forks a child process and executes the given function in the child.
     * The parent continues and tracks the child PID.
     * Returns the PID in parent, or -1 on error.
     */
    template <typename Func> pid_t forkAndRun(Func&& func)
    {
        _pid = fork();
        if (_pid == 0)
        {
            // Child process
            func();
            _exit(0); // Ensure child exits if func returns
        }
        else if (_pid > 0)
        {
            // Parent process
            return _pid;
        }
        else
        {
            // Fork failed
            return -1;
        }
    }

    /**
     * Checks the status of the child process without blocking.
     */
    Status checkStatus(int* exitCode = nullptr)
    {
        if (_pid <= 0)
            return Status::NONE;

        int status;
        pid_t result = waitpid(_pid, &status, WNOHANG);

        if (result == 0)
        {
            return Status::RUNNING;
        }
        else if (result == _pid)
        {
            if (WIFEXITED(status))
            {
                if (exitCode)
                    *exitCode = WEXITSTATUS(status);
                _pid = -1;
                return Status::EXITED;
            }
            if (WIFSIGNALED(status))
            {
                if (exitCode)
                    *exitCode = WTERMSIG(status);
                _pid = -1;
                return Status::SIGNALED;
            }
            _pid = -1;
            return Status::ERROR;
        }
        else
        {
            return Status::ERROR;
        }
    }

    /**
     * Forcefully kills the child process.
     */
    bool killChild(int signal = SIGTERM)
    {
        if (_pid <= 0)
            return false;
        if (kill(_pid, signal) == 0)
        {
            waitpid(_pid, nullptr, 0); // Collect zombie
            _pid = -1;
            return true;
        }
        return false;
    }

    pid_t getPid() const
    {
        return _pid;
    }

    bool isRunning()
    {
        return checkStatus() == Status::RUNNING;
    }

private:
    pid_t _pid = -1;
};
