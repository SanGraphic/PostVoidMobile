using System;
using System.Reflection;
using System.IO;

class ScriptScanner {
    static void Main() {
        var asm = Assembly.LoadFrom("UndertaleModTool_v0.9.2.0-Windows-SingleFile/UndertaleModLib.dll");
        foreach (var res in asm.GetManifestResourceNames()) {
            Console.WriteLine("Res: " + res);
        }
    }
}
