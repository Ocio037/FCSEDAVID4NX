using System;
using System.IO;
namespace ArcPacker
{
    public class Program
    {
        public static int Main(string[] args)
        {
            if (args.Length < 2)
            {
                PrintUsage();
                return 1;
            }
            if (args[0] == "-u" || args[0] == "--unpack")
            {
                if (args.Length < 3)
                {
                    Console.WriteLine("Uso para desempaquetar: ArcPacker -u <archivo.dat> <carpeta_destino>");
                    return 1;
                }
                ArchivePacker.Unpack(args[1], args[2]);
                return 0;
            }
            string csvPath = args[0];
            string baseDirectory = args[1];
            string outputDatPath;
            if (args.Length >= 3 && !string.IsNullOrWhiteSpace(args[2]))
            {
                outputDatPath = args[2];
            }
            else
            {
                outputDatPath = Path.ChangeExtension(csvPath, ".dat");
            }
            outputDatPath = Path.ChangeExtension(outputDatPath, ".dat");
            try
            {
                Console.WriteLine($"CSV                 : {Path.GetFullPath(csvPath)}");
                Console.WriteLine($"res                 : {Path.GetFullPath(baseDirectory)}");
                Console.WriteLine($"dest                : {Path.GetFullPath(outputDatPath)}");
                Console.WriteLine();
                ArchivePacker.Pack(csvPath, baseDirectory, outputDatPath);
                return 0;
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine();
                Console.WriteLine($"[ERROR] {ex.Message}");
                Console.ResetColor();
                return 1;
            }
        }
        private static void PrintUsage()
        {
            Console.WriteLine("Uso (Empaquetar):");
            Console.WriteLine("  ArcPacker <archivo.csv> <carpeta_de_archivos> [salida.dat]");
            Console.WriteLine();
            Console.WriteLine("Argumentos:");
            Console.WriteLine("  1. <archivo.csv>        : CSV con nombre, ID y 4 argumentos u32; NEON conserva nombre y orden.");
            Console.WriteLine("  2. <carpeta_de_archivos>: Carpeta donde estan ubicados los archivos a empaquetar.");
            Console.WriteLine("  3. [salida.dat]         : (Opcional) Ruta del archivo NEON .dat de salida.");
            Console.WriteLine("                            Por defecto se genera con el nombre del CSV y extension .dat.");
            Console.WriteLine("                            Guarda nombres, no IDs ni argumentos CSV.");
            Console.WriteLine("                            El ID usado por el runtime es el indice de entrada.");
            Console.WriteLine();
            Console.WriteLine("Uso (Desempaquetar / Verificar):");
            Console.WriteLine("  ArcPacker -u <archivo.dat> <carpeta_destino>");
            Console.WriteLine();
            Console.WriteLine("Ejemplo:");
            Console.WriteLine("  ArcPacker lista.csv ./recursos paquete.dat");
        }
    }
}