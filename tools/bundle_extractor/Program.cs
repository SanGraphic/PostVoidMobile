using System;
using System.IO;
using SingleFileExtractor.Core;

class Program
{
    static void Main(string[] args)
    {
        string exePath = @"C:\Users\sanay\PostVoid\UndertaleModTool_v0.9.2.0-Windows-SingleFile\UndertaleModTool.exe";
        string outputDir = @"C:\Users\sanay\PostVoid\tools\umt_extracted";

        if (!Directory.Exists(outputDir))
        {
            Directory.CreateDirectory(outputDir);
        }

        Console.WriteLine($"Extracting {exePath} to {outputDir}...");
        var extractor = new ExecutableReader(exePath);
        if (extractor.IsSupported)
        {
            var bundle = extractor.Bundle;
            Console.WriteLine($"Bundle found with {bundle.Files.Count} files.");
            foreach (var file in bundle.Files)
            {
                if (file.Type == FileType.Assembly || file.Type == FileType.DepsJson || file.Type == FileType.RuntimeConfigJson)
                {
                    string targetFile = Path.Combine(outputDir, file.RelativePath);
                    string? dir = Path.GetDirectoryName(targetFile);
                    if (dir != null && !Directory.Exists(dir))
                        Directory.CreateDirectory(dir);

                    using var stream = file.AsStream();
                    using var fs = File.Create(targetFile);
                    stream.CopyTo(fs);
                }
            }
            Console.WriteLine("Extraction complete!");
        }
        else
        {
            Console.WriteLine("Executable not supported by SingleFileExtractor.");
        }
    }
}
