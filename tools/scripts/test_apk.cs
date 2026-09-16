using System;
using System.IO.Compression;

class CheckApk {
    static void Main() {
        string apk = @"C:\Users\sanay\Downloads\LooPatHerO_demo.apk";
        using var zip = ZipFile.OpenRead(apk);
        foreach (var entry in zip.Entries) {
            if (entry.FullName.Contains("yoyo") || entry.FullName.Contains("droid") || entry.FullName.EndsWith(".so")) {
                Console.WriteLine("Entry: " + entry.FullName + " (" + entry.Length + " bytes)");
            }
        }
    }
}
