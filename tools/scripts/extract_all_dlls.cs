using System;
using System.IO;
using SingleFileExtractor.Core;

class Extractor {
    static void Main() {
        string exe = @"UndertaleModTool_v0.9.2.0-Windows-SingleFile\UndertaleModTool.exe";
        string outDir = @"UndertaleModTool_v0.9.2.0-Windows-SingleFile";
        var bundle = new ExecutableReader(exe).Read();
        if (bundle != null) {
            foreach (var entry in bundle.Files) {
                if (entry.RelativePath.EndsWith(".dll") || entry.RelativePath.EndsWith(".so")) {
                    string dest = Path.Combine(outDir, entry.RelativePath);
                    Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
                    using var stream = entry.Open();
                    using var file = File.Create(dest);
                    stream.CopyTo(file);
                    Console.WriteLine("Extracted: " + entry.RelativePath);
                }
            }
        }
    }
}
