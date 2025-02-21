// Program.cs
// FLP Project 1: ECDSA (tests)
// Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
// Year: 2023

public static class Program
{
    const string TestDir = "test";
    static readonly string CurvesDir = Path.Combine(TestDir, "curves");
    static readonly string TmpDataDir = Path.Combine(TestDir, "testdir");
    static string? Exe = null;
    static Random? rnd;

    const int KeysPerCurve = 15;
    const int MessagesPerKey = 10;
    const int PseudoHashBits = 256;
    const bool ExitOnError = false;

    public static async Task Main(string[] args)
    {
        if (args.Length > 0)
        {
            Exe = args[0];
        }

        if (args.Length > 1)
        {
            rnd = new Random(int.Parse(args[1]));
        }
        else
        {
            rnd = new Random();
        }

        if (!Directory.Exists(TmpDataDir))
        {
            Directory.CreateDirectory(TmpDataDir);
        }

        var curves = Directory.GetFiles(CurvesDir);
        foreach (var curve in curves)
        {
            Console.WriteLine("Checking curve " + Path.GetFileName(curve));
            var (res, curveContents) = await TestCurveIO(curve);
            if (!res) continue;

            await TestFlow(curve, curveContents);
        }
    }

    public static async Task TestFlow(string curvePath, string curveContents)
    {
        for (var k = 0; k < KeysPerCurve; k++)
        {
            Console.Write($"> Key {k + 1} / {KeysPerCurve} .. ");
            var key = await MakeKey(curvePath);
            if (key == null)
                continue;

            var ok = true;
            for (var m = 0; m < MessagesPerKey; m++)
            {
                ok &= await TestSignValidate(curveContents, key);
            }

            if(ok)
                Console.WriteLine("OK");
        }
    }

    public static async Task<bool> TestSignValidate(string curveContents, string key)
    {
        var h = MakePseudoHash();
        var path = Path.Combine(TmpDataDir, h);
        var validatePath = Path.Combine(TmpDataDir, h + "-validate");

        var f = new StreamWriter(path);
        await f.WriteAsync(curveContents);
        await f.WriteAsync(key);
        await f.WriteAsync("Hash: 0x");
        await f.WriteLineAsync(h);
        await f.FlushAsync();
        await f.DisposeAsync();

        var (signExitCode, signResult) = await Utils.RunCollect(Exe, $"-s \"{path}\"");
        if (signExitCode != 0)
        {
            Console.WriteLine($"Invalid SIGN result (exit code {signExitCode}) for {h}");
            return false;
        }

        var rc = await TestValidateCorrect(curveContents, signResult, key, h);
        if (rc)
        {
            var ri1 = await TestValidateIncorrectHash(curveContents, signResult, key, h);

            if (ri1)
            {
                var ri2 = await TestValidateIncorrectKey(curveContents, signResult, key, h);

                if (ri2)
                {
                    File.Delete(path);
                    return true;
                }
            }
        }

        return false;
    }

    public static async Task<bool> TestValidate(string curveContents, string sig, string pubkey, string h, bool correct, string test)
    {
        var validatePath = Path.Combine(TmpDataDir, $"{h}-validate-{test}");

        var f = new StreamWriter(validatePath);

        await f.WriteAsync(curveContents);
        await f.WriteAsync(sig);
        await f.WriteLineAsync($"PublicKey {{\nQ: {pubkey}\n}}");
        await f.WriteLineAsync($"Hash: 0x{h}");
        await f.FlushAsync();
        await f.DisposeAsync();

        var (validateExitCode, validateResult) = await Utils.RunCollect(Exe, $"-v \"{validatePath}\"");
        if ((Sanitize(validateResult) == "True" && validateExitCode == 0) == correct)
        {
            File.Delete(validatePath);
            return true;
        }
        else
        {
            Console.WriteLine($"Invalid VALIDATE ({(correct ? "expected True" : "expected False")}) result (exit code {validateExitCode}) for {h}");

            if (ExitOnError)
                Environment.Exit(1);

            return false;
        }

    }

    public static async Task<bool> TestValidateCorrect(string curveContents, string sig, string key, string h)
    {
        return await TestValidate(curveContents, sig, GetPubKey(key), h, true, "correct");
    }

    public static async Task<bool> TestValidateIncorrectKey(string curveContents, string sig, string key, string h)
    {
        var k = GetPubKey(key);
        k = ChangeRandomHexDigits(k, 4);
        return await TestValidate(curveContents, sig, k, h, false, "badkey");
    }

    public static async Task<bool> TestValidateIncorrectHash(string curveContents, string sig, string key, string h)
    {
        h = ChangeRandomHexDigits(h, 0);
        return await TestValidate(curveContents, sig, GetPubKey(key), h, false, "badhash");
    }


    public static async Task<(bool, string)> TestCurveIO(string curvePath)
    {
        var (ec, stdout) = await Utils.RunCollect(Exe, $"-i \"{curvePath}\"");
        var curve = await File.ReadAllTextAsync(curvePath);

        if (ec != 0 || !Sanitize(curve).Equals(Sanitize(stdout), StringComparison.InvariantCultureIgnoreCase))
        {
            Console.WriteLine($"Invalid output (exit code {ec}) for curve: {curvePath}");
            return (false, string.Empty);
        }

        return (true, stdout);
    }

    public static async Task<string?> MakeKey(string curvePath)
    {
        var (ec, res) = await Utils.RunCollect(Exe, $"-k \"{curvePath}\"");
        if (ec != 0)
        {
            Console.WriteLine($"Cannot make key (exit code {ec}) for curve: {curvePath}");
            return null;
        }

        return res;
    }

    public static string MakePseudoHash()
    {
        var b = new byte[PseudoHashBits / 8];
        rnd.NextBytes(b);
        return Convert.ToHexString(b);
    }

    public static string GetPubKey(string key)
    {
        var q = key.IndexOf('Q');
        var endl = key.IndexOf('\n', q);
        return key[(q + 2)..endl].Trim();
    }

    public static string ChangeRandomHexDigits(string s, int start)
    {
        var sa = s.ToArray();
        var change = false;

        while (!change)
        {
            for (var i = start; i < s.Length; i++)
            {
                if (rnd.Next(2) == 1)
                {
                    var orig = sa[i];
                    while (sa[i] == orig)
                    {
                        sa[i] = rnd.Next(16).ToString("x")[0];
                    }
                    change = true;
                }
            }
        }

        return new String(sa);
    }

    public static string Sanitize(string s) => s.ReplaceLineEndings("\n").Replace("\t", "").Replace(" ", "").Trim();
}

