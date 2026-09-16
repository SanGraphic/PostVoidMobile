using System;
using System.Reflection;
using System.Linq;

class TestReflection {
    static void Main() {
        var asm = Assembly.LoadFrom("UndertaleModTool_v0.9.2.0-Windows-SingleFile/UndertaleModLib.dll");
        var types = asm.GetTypes();
        var exportTypes = types.Where(t => t.Name.Contains("Export") || t.Name.Contains("YYP") || t.Name.Contains("Project") || t.Name.Contains("Dump")).ToList();
        foreach (var t in exportTypes) {
            Console.WriteLine(t.FullName);
        }
    }
}
