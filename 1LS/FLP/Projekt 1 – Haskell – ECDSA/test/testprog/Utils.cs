// Utils.cs
// FLP Project 1: ECDSA (tests)
// Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
// Year: 2023

using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;

public static class Utils
{
    public static async Task<(int, string)> RunCollect(string? exe, string args)
    {
        ProcessStartInfo psi;
        if (exe == null)
        {
            psi = new ProcessStartInfo("stack", "run -- " + args) { RedirectStandardOutput = true };

        }
        else
        {
            psi = new ProcessStartInfo(exe, args) { RedirectStandardOutput = true };
        }

        await using var stdoutWriter = new StringWriter();
        var res = await Utils.StartProcess(psi, exe == null ? 10000 : 1000, stdoutWriter);

        await stdoutWriter.FlushAsync();
        var stdout = stdoutWriter.ToString();
        return (res, stdout);
    }

    /// <summary>
    /// Asynchronously starts a process and reads its stdout and stderr in different threads.
    /// The asynchronous operation terminates when the process has ended and all the data has been read.
    /// </summary>
    /// <param name="startInfo">The <see cref="ProcessStartInfo"/> describing the process to start.
    /// Its <see cref="ProcessStartInfo.RedirectStandardOutput"/> and <see cref="ProcessStartInfo.RedirectStandardError"/>
    /// must be set to true if the corresponding <see cref="TextWriter"/> argument is not null.</param>
    /// <param name="timeout">The maximum time the process may run for, or null for no timeout.</param>
    /// <param name="outputTextWriter">A <see cref="TextWriter"/> to write the stdout to, or null if it isn't redirected.</param>
    /// <param name="errorTextWriter">A <see cref="TextWriter"/> to write the stderr to, or null if it isn't redirected.</param>
    /// <returns>A task that will complete when the process exits and the buffers are read. Its result value is the
    /// exit code of the process.</returns>
    /// <remarks>Original source: https://stackoverflow.com/a/39872058. Original author: Muhammad Rehan Saeed. Modified
    /// by Ondřej Ondryáš. Licensed under CC BY-SA 4.0</remarks>
    [SuppressMessage("ReSharper", "AccessToDisposedClosure")]
    public static async Task<int> StartProcess(ProcessStartInfo startInfo, int? timeout = null,
        TextWriter? outputTextWriter = null, TextWriter? errorTextWriter = null)
    {
        using var process = new Process() { StartInfo = startInfo };
        var cancellationTokenSource = timeout.HasValue
            ? new CancellationTokenSource(timeout.Value)
            : new CancellationTokenSource();

        process.Start();

        var tasks = new List<Task>(3) { process.WaitForExitAsync(cancellationTokenSource.Token) };
        if (outputTextWriter != null)
        {
            tasks.Add(ReadAsync(x =>
                {
                    process.OutputDataReceived += x;
                    process.BeginOutputReadLine();
                },
                x => process.OutputDataReceived -= x,
                outputTextWriter,
                cancellationTokenSource.Token));
        }

        if (errorTextWriter != null)
        {
            tasks.Add(ReadAsync(x =>
                {
                    process.ErrorDataReceived += x;
                    process.BeginErrorReadLine();
                },
                x => process.ErrorDataReceived -= x,
                errorTextWriter,
                cancellationTokenSource.Token));
        }

        await Task.WhenAll(tasks);
        cancellationTokenSource.Dispose();

        return process.ExitCode;
    }

    /// <summary>
    /// Reads the data from the specified data received event and writes it to the
    /// <paramref name="textWriter"/>.
    /// </summary>
    /// <param name="addHandler">Adds the event handler.</param>
    /// <param name="removeHandler">Removes the event handler.</param>
    /// <param name="textWriter">The text writer.</param>
    /// <param name="cancellationToken">The cancellation token.</param>
    /// <returns>A task representing the asynchronous operation.</returns>
    /// <remarks>Original source: https://stackoverflow.com/a/39872058. Original author: Muhammad Rehan Saeed. Modified
    /// by Ondřej Ondryáš. Licensed under CC BY-SA 4.0</remarks>
    private static Task ReadAsync(this Action<DataReceivedEventHandler> addHandler,
        Action<DataReceivedEventHandler> removeHandler, TextWriter textWriter,
        CancellationToken cancellationToken = default)
    {
        var taskCompletionSource = new TaskCompletionSource();

        DataReceivedEventHandler handler = null!;

        handler = (_, e) =>
        {
            if (e.Data == null)
            {
                removeHandler(handler);
                taskCompletionSource.TrySetResult();
            }
            else
            {
                textWriter.WriteLine(e.Data);
            }
        };

        addHandler(handler);

        if (cancellationToken != default)
        {
            cancellationToken.Register(() =>
            {
                removeHandler(handler);
                taskCompletionSource.TrySetCanceled();
            });
        }

        return taskCompletionSource.Task;
    }
}
