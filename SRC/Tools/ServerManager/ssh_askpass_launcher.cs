// Developer-mode launcher. The frozen manager is its own native askpass helper.
// Forward prompts as Base64 data without invoking a command shell.
using System;
using System.Diagnostics;
using System.Text;

class AurigaSshAskpass {
    static int Main(string[] args) {
        try {
            if (args.Length != 1) return 1;
            string python = Environment.GetEnvironmentVariable("AURIGA_ASKPASS_PYTHON");
            string script = Environment.GetEnvironmentVariable("AURIGA_ASKPASS_SCRIPT");
            if (String.IsNullOrEmpty(python) || String.IsNullOrEmpty(script) || script.Contains("\"")) return 1;
            var start = new ProcessStartInfo(python, "-B \"" + script + "\" --prompt-base64 " +
                Convert.ToBase64String(Encoding.UTF8.GetBytes(args[0])));
            start.UseShellExecute = false;
            start.CreateNoWindow = true;
            start.RedirectStandardInput = true;
            start.RedirectStandardOutput = true;
            start.RedirectStandardError = true;
            using (var child = Process.Start(start)) {
                child.StandardInput.Close();
                child.ErrorDataReceived += delegate { };
                child.BeginErrorReadLine();
                var output = Console.OpenStandardOutput();
                child.StandardOutput.BaseStream.CopyTo(output);
                output.Flush();
                child.WaitForExit();
                return child.ExitCode;
            }
        } catch { return 1; }
    }
}
