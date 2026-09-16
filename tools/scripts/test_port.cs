using System;
using System.IO.Compression;

class PortChecker {
    static void Main() {
        string portFile = @"postvoidportmasterport\postvoid\postvoid.port";
        using var zip = ZipFile.OpenRead(portFile);
        foreach (var entry in zip.Entries) {
            Console.WriteLine($"Port Entry: {entry.FullName} ({entry.Length} bytes)");
        }
    }
}
