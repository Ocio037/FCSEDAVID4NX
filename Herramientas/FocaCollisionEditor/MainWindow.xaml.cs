using Foca3DEditor.Models;
using Microsoft.Win32;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace FocaCollisionEditor
{
    public partial class MainWindow : Window
    {
        private readonly ObservableCollection<CollisionVolume> Collisions = new();
        private Fc3dFile? Model;
        private string? ModelPath;
        private string? CollisionPath;
        private string AssociatedModelName = string.Empty;
        private bool UpdatingInspector;
        private Vector3 ModelCenter;
        private Vector3 ModelSize = Vector3.One;
        private Vector3 PanOffset;
        private float ModelRadius = 1.0f;
        private float CameraDistance = 4.0f;
        private float CameraYaw = 1.10715f;
        private float CameraPitch = 0.42053f;
        private Point LastMousePosition;
        private MouseButton DragButton;
        private float[]? ViewportDepthBuffer;
        private int ViewportPixelWidth;
        private int ViewportPixelHeight;

        public MainWindow()
        {
            InitializeComponent();
            CollisionList.ItemsSource = Collisions;
        }

        private void OpenModel_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Title = "Abrir modelo Foca 3D",
                Filter = "Modelos Foca 3D (*.f3d)|*.f3d|Todos los archivos (*.*)|*.*"
            };
            if (dialog.ShowDialog(this) != true)
            {
                return;
            }

            try
            {
                var model = Fc3dFile.Read(dialog.FileName);
                if (model.Meshes.Count == 0)
                {
                    throw new InvalidDataException("El archivo no contiene mallas.");
                }
                Model = model;
                ModelPath = dialog.FileName;
                AssociatedModelName = System.IO.Path.GetFileName(ModelPath);
                UpdateModelLabel();
                EmptyViewText.Visibility = Visibility.Collapsed;
                LoadCollisions();
                CameraYaw = 1.10715f;
                CameraPitch = 0.42053f;
                FrameModel();
                SetStatus($"{model.Meshes.Count} mallas · {model.Meshes.Sum(mesh => mesh.Vertices.Count)} vértices · {CollisionSidecarPath()}");
                RenderViewport();
            }
            catch (Exception exception)
            {
                MessageBox.Show(this, $"No se pudo abrir el modelo:\n{exception.Message}", "Foca Collision Editor", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void LoadCollisions()
        {
            Collisions.Clear();
            AssociatedModelName = string.IsNullOrEmpty(ModelPath)
                ? AssociatedModelName
                : System.IO.Path.GetFileName(ModelPath);
            UpdateModelLabel();
            string path = CollisionSidecarPath();
            CollisionPath = path;
            if (string.IsNullOrEmpty(path) || !System.IO.File.Exists(path))
            {
                SetInspectorFromSelection();
                return;
            }

            try
            {
                var document = FcolFile.Read(path);
                foreach (var collision in document.Collisions)
                {
                    Collisions.Add(collision);
                }
                if (!string.IsNullOrEmpty(document.Model) && !string.Equals(document.Model, System.IO.Path.GetFileName(ModelPath), StringComparison.OrdinalIgnoreCase))
                {
                    SetStatus($"Aviso: el .fcol indica que pertenece a {document.Model}.");
                }
            }
            catch (Exception exception)
            {
                MessageBox.Show(this, $"No se pudo leer el archivo de colisiones:\n{exception.Message}", "Foca Collision Editor", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
            SetInspectorFromSelection();
            UpdateCollisionOverlay();
        }

        private void SaveCollisions_Click(object sender, RoutedEventArgs e)
        {
            SaveCollisions();
        }

        private void SaveCollisions()
        {
            if (string.IsNullOrEmpty(CollisionPath))
            {
                SetStatus("No hay una ruta .fcol seleccionada. Elige dónde guardar el archivo.");
                string? newPath = ShowSaveFcolDialog("Guardar colisiones");
                if (string.IsNullOrEmpty(newPath))
                {
                    return;
                }
                ApplyInspectorChanges();
                ExportCollisions(newPath);
                return;
            }
            ApplyInspectorChanges();
            string path = CollisionPath ?? CollisionSidecarPath();
            ExportCollisions(path);
        }

        private void ImportFcol_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Title = Model == null ? "Abrir archivo de colisiones" : "Importar colisiones",
                Filter = "Colisiones Foca (*.fcol)|*.fcol|Todos los archivos (*.*)|*.*"
            };
            if (dialog.ShowDialog(this) != true)
            {
                return;
            }
            try
            {
                var document = FcolFile.Read(dialog.FileName);
                if (Model != null && !string.IsNullOrEmpty(ModelPath))
                {
                    string modelName = System.IO.Path.GetFileName(ModelPath);
                    if (!string.IsNullOrEmpty(document.Model) && !string.Equals(document.Model, modelName, StringComparison.OrdinalIgnoreCase))
                    {
                        var result = MessageBox.Show(this,
                            $"Este archivo fue exportado para '{document.Model}', pero el modelo abierto es '{modelName}'.\n¿Quieres importar las colisiones de todos modos?",
                            "Modelo distinto", MessageBoxButton.YesNo, MessageBoxImage.Warning);
                        if (result != MessageBoxResult.Yes)
                        {
                            return;
                        }
                    }
                }
                else
                {
                    AssociatedModelName = document.Model;
                    UpdateModelLabel();
                }
                Collisions.Clear();
                foreach (var collision in document.Collisions)
                {
                    Collisions.Add(collision);
                }
                CollisionPath = dialog.FileName;
                if (Model == null)
                {
                    CameraYaw = 1.10715f;
                    CameraPitch = 0.42053f;
                    FrameCollisions();
                }
                SetInspectorFromSelection();
                UpdateEmptyViewText();
                RenderViewport();
                SetStatus($"Importadas {Collisions.Count} colisiones desde {CollisionPath}.");
            }
            catch (Exception exception)
            {
                MessageBox.Show(this, $"No se pudieron importar las colisiones:\n{exception.Message}", "Foca Collision Editor", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void ExportFcol_Click(object sender, RoutedEventArgs e)
        {
            ApplyInspectorChanges();
            string? path = ShowSaveFcolDialog("Exportar colisiones");
            if (!string.IsNullOrEmpty(path))
            {
                ExportCollisions(path);
            }
        }

        private void ExportCollisions(string path)
        {
            try
            {
                if (!string.IsNullOrEmpty(ModelPath))
                {
                    AssociatedModelName = System.IO.Path.GetFileName(ModelPath);
                }
                FcolFile.Write(path, Collisions.ToList());
                CollisionPath = path;
                UpdateModelLabel();
                SetStatus($"Exportadas {Collisions.Count} colisiones: {path}");
            }
            catch (Exception exception)
            {
                MessageBox.Show(this, $"No se pudieron exportar las colisiones:\n{exception.Message}", "Foca Collision Editor", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private string? ShowSaveFcolDialog(string title)
        {
            string defaultPath = CollisionPath ?? CollisionSidecarPath();
            string fileName = !string.IsNullOrEmpty(defaultPath)
                ? System.IO.Path.GetFileName(defaultPath)
                : !string.IsNullOrEmpty(AssociatedModelName)
                    ? System.IO.Path.ChangeExtension(AssociatedModelName, ".fcol")
                    : "collisions.fcol";
            var dialog = new SaveFileDialog
            {
                Title = title,
                Filter = "Colisiones Foca (*.fcol)|*.fcol|Todos los archivos (*.*)|*.*",
                DefaultExt = ".fcol",
                AddExtension = true,
                FileName = fileName
            };
            string? directory = System.IO.Path.GetDirectoryName(defaultPath);
            if (!string.IsNullOrEmpty(directory))
            {
                dialog.InitialDirectory = directory;
            }
            return dialog.ShowDialog(this) == true ? dialog.FileName : null;
        }

        private string CollisionSidecarPath()
        {
            if (string.IsNullOrEmpty(ModelPath))
            {
                return string.Empty;
            }
            return System.IO.Path.ChangeExtension(ModelPath, ".fcol");
        }

        private void AddAabb_Click(object sender, RoutedEventArgs e)
        {
            AddCollision(CollisionShape.AABB);
        }

        private void AddObb_Click(object sender, RoutedEventArgs e)
        {
            AddCollision(CollisionShape.OBB);
        }

        private void AddCollision(CollisionShape shape)
        {
            float minSize = Math.Max(ModelRadius * 0.2f, 0.1f);
            Vector3 center = ModelCenter;
            Vector3 size = Model == null && Collisions.Count == 0
                ? Vector3.One
                : new Vector3(
                    Math.Max(ModelSize.X * 0.5f, minSize),
                    Math.Max(ModelSize.Y * 0.5f, minSize),
                    Math.Max(ModelSize.Z * 0.5f, minSize));
            var collision = new CollisionVolume
            {
                Id = NextCollisionId(),
                Shape = shape,
                Kind = CollisionKind.Solid,
                CenterX = center.X,
                CenterY = center.Y,
                CenterZ = center.Z,
                SizeX = size.X,
                SizeY = size.Y,
                SizeZ = size.Z
            };
            Collisions.Add(collision);
            CollisionList.SelectedItem = collision;
            UpdateEmptyViewText();
            SetStatus($"Añadida colisión con ID {collision.Id}.");
            UpdateCollisionOverlay();
        }

        private uint NextCollisionId()
        {
            var usedIds = Collisions.Select(collision => collision.Id).ToHashSet();
            uint candidate = 0;
            while (usedIds.Contains(candidate) && candidate < uint.MaxValue)
            {
                ++candidate;
            }
            return candidate;
        }

        private void DeleteCollision_Click(object sender, RoutedEventArgs e)
        {
            DeleteSelectedCollision();
        }

        private void DeleteSelectedCollision()
        {
            if (CollisionList.SelectedItem is not CollisionVolume collision)
            {
                return;
            }
            int index = CollisionList.SelectedIndex;
            Collisions.Remove(collision);
            if (Collisions.Count > 0)
            {
                CollisionList.SelectedIndex = Math.Min(index, Collisions.Count - 1);
            }
            UpdateEmptyViewText();
            SetStatus($"Eliminada colisión con ID {collision.Id}.");
            UpdateCollisionOverlay();
        }

        private void CollisionList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            SetInspectorFromSelection();
            UpdateCollisionOverlay();
        }

        private void SetInspectorFromSelection()
        {
            UpdatingInspector = true;
            bool hasCollision = CollisionList.SelectedItem is CollisionVolume;
            IdTextBox.IsEnabled = hasCollision;
            ShapeCombo.IsEnabled = hasCollision;
            KindCombo.IsEnabled = hasCollision;
            CenterXTextBox.IsEnabled = hasCollision;
            CenterYTextBox.IsEnabled = hasCollision;
            CenterZTextBox.IsEnabled = hasCollision;
            SizeXTextBox.IsEnabled = hasCollision;
            SizeYTextBox.IsEnabled = hasCollision;
            SizeZTextBox.IsEnabled = hasCollision;
            RotationXTextBox.IsEnabled = hasCollision;
            RotationYTextBox.IsEnabled = hasCollision;
            RotationZTextBox.IsEnabled = hasCollision;

            if (CollisionList.SelectedItem is CollisionVolume collision)
            {
                IdTextBox.Text = collision.Id.ToString(CultureInfo.InvariantCulture);
                ShapeCombo.SelectedIndex = collision.Shape == CollisionShape.OBB ? 1 : 0;
                KindCombo.SelectedIndex = collision.Kind == CollisionKind.Trigger ? 1 : 0;
                CenterXTextBox.Text = Format(collision.CenterX);
                CenterYTextBox.Text = Format(collision.CenterY);
                CenterZTextBox.Text = Format(collision.CenterZ);
                SizeXTextBox.Text = Format(collision.SizeX);
                SizeYTextBox.Text = Format(collision.SizeY);
                SizeZTextBox.Text = Format(collision.SizeZ);
                RotationXTextBox.Text = Format(collision.RotationX);
                RotationYTextBox.Text = Format(collision.RotationY);
                RotationZTextBox.Text = Format(collision.RotationZ);
                InspectorHintText.Text = "Tamaño completo del volumen. AABB permanece alineada a los ejes; OBB permite rotación.";
            }
            else
            {
                IdTextBox.Clear();
                CenterXTextBox.Clear();
                CenterYTextBox.Clear();
                CenterZTextBox.Clear();
                SizeXTextBox.Clear();
                SizeYTextBox.Clear();
                SizeZTextBox.Clear();
                RotationXTextBox.Clear();
                RotationYTextBox.Clear();
                RotationZTextBox.Clear();
                ShapeCombo.SelectedIndex = -1;
                KindCombo.SelectedIndex = -1;
                InspectorHintText.Text = "Selecciona una colisión para editarla.";
            }
            UpdatingInspector = false;
            UpdateRotationState();
        }

        private void Inspector_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (!UpdatingInspector)
            {
                ApplyInspectorChanges();
            }
        }

        private void Inspector_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!UpdatingInspector)
            {
                ApplyInspectorChanges();
            }
        }

        private void ApplyInspectorChanges()
        {
            if (UpdatingInspector || CollisionList.SelectedItem is not CollisionVolume collision)
            {
                return;
            }

            if (uint.TryParse(IdTextBox.Text, NumberStyles.None, CultureInfo.InvariantCulture, out uint id))
            {
                bool idInUse = Collisions.Any(other => !ReferenceEquals(other, collision) && other.Id == id);
                if (idInUse)
                {
                    InspectorHintText.Text = $"El ID {id} ya está asignado a otra colisión.";
                }
                else
                {
                    collision.Id = id;
                    InspectorHintText.Text = "ID único. Tamaño completo del volumen. AABB permanece alineada a los ejes; OBB permite rotación.";
                }
            }
            if (ShapeCombo.SelectedIndex >= 0)
            {
                collision.Shape = ShapeCombo.SelectedIndex == 1 ? CollisionShape.OBB : CollisionShape.AABB;
            }
            if (KindCombo.SelectedIndex >= 0)
            {
                collision.Kind = KindCombo.SelectedIndex == 1 ? CollisionKind.Trigger : CollisionKind.Solid;
            }
            SetFloat(CenterXTextBox.Text, value => collision.CenterX = value);
            SetFloat(CenterYTextBox.Text, value => collision.CenterY = value);
            SetFloat(CenterZTextBox.Text, value => collision.CenterZ = value);
            SetFloat(SizeXTextBox.Text, value => collision.SizeX = Math.Max(Math.Abs(value), 0.01f));
            SetFloat(SizeYTextBox.Text, value => collision.SizeY = Math.Max(Math.Abs(value), 0.01f));
            SetFloat(SizeZTextBox.Text, value => collision.SizeZ = Math.Max(Math.Abs(value), 0.01f));
            if (collision.Shape == CollisionShape.OBB)
            {
                SetFloat(RotationXTextBox.Text, value => collision.RotationX = value);
                SetFloat(RotationYTextBox.Text, value => collision.RotationY = value);
                SetFloat(RotationZTextBox.Text, value => collision.RotationZ = value);
            }
            else
            {
                collision.RotationX = 0.0f;
                collision.RotationY = 0.0f;
                collision.RotationZ = 0.0f;
                UpdatingInspector = true;
                RotationXTextBox.Text = Format(0.0f);
                RotationYTextBox.Text = Format(0.0f);
                RotationZTextBox.Text = Format(0.0f);
                UpdatingInspector = false;
            }

            CollisionList.Items.Refresh();
            UpdateRotationState();
            UpdateCollisionOverlay();
        }

        private void UpdateRotationState()
        {
            bool canRotate = CollisionList.SelectedItem is CollisionVolume collision && collision.Shape == CollisionShape.OBB;
            RotationXTextBox.IsEnabled = canRotate;
            RotationYTextBox.IsEnabled = canRotate;
            RotationZTextBox.IsEnabled = canRotate;
        }

        private static void SetFloat(string text, Action<float> setValue)
        {
            if (float.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out float value) ||
                float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value))
            {
                if (float.IsFinite(value))
                {
                    setValue(value);
                }
            }
        }

        private static string Format(float value)
        {
            return value.ToString("0.###", CultureInfo.CurrentCulture);
        }

        private void FrameModel()
        {
            if (Model == null)
            {
                return;
            }
            Vector3 minimum = new(float.MaxValue);
            Vector3 maximum = new(float.MinValue);
            bool hasVertices = false;
            foreach (var mesh in Model.Meshes)
            {
                foreach (var vertex in mesh.Vertices)
                {
                    minimum = Vector3.Min(minimum, vertex.Position);
                    maximum = Vector3.Max(maximum, vertex.Position);
                    hasVertices = true;
                }
            }
            if (!hasVertices)
            {
                minimum = Vector3.Zero;
                maximum = Vector3.One;
            }

            ModelCenter = (minimum + maximum) * 0.5f;
            ModelSize = Vector3.Max(maximum - minimum, new Vector3(0.1f));
            ModelRadius = Math.Max(ModelSize.Length() * 0.5f, 0.1f);
            CameraDistance = ModelRadius / MathF.Tan(MathF.PI / 6.0f) * 1.35f;
            PanOffset = Vector3.Zero;
        }

        private void FrameCollisions()
        {
            if (Collisions.Count == 0)
            {
                ModelCenter = Vector3.Zero;
                ModelSize = Vector3.One;
                ModelRadius = 1.0f;
            }
            else
            {
                Vector3 minimum = new(float.MaxValue);
                Vector3 maximum = new(float.MinValue);
                foreach (var collision in Collisions)
                {
                    Vector3 halfSize = collision.Size * 0.5f;
                    minimum = Vector3.Min(minimum, collision.Center - halfSize);
                    maximum = Vector3.Max(maximum, collision.Center + halfSize);
                }
                ModelCenter = (minimum + maximum) * 0.5f;
                ModelSize = Vector3.Max(maximum - minimum, new Vector3(0.1f));
                ModelRadius = Math.Max(ModelSize.Length() * 0.5f, 0.1f);
            }
            CameraDistance = ModelRadius / MathF.Tan(MathF.PI / 6.0f) * 1.35f;
            PanOffset = Vector3.Zero;
        }

        private void UpdateModelLabel()
        {
            if (!string.IsNullOrEmpty(ModelPath))
            {
                ModelNameText.Text = System.IO.Path.GetFileName(ModelPath);
            }
            else if (!string.IsNullOrEmpty(AssociatedModelName))
            {
                ModelNameText.Text = $"Modelo asociado: {AssociatedModelName}";
            }
            else
            {
                ModelNameText.Text = "Sin modelo 3D";
            }
        }

        private void UpdateEmptyViewText()
        {
            if (Model != null || Collisions.Count > 0)
            {
                EmptyViewText.Visibility = Visibility.Collapsed;
                return;
            }
            EmptyViewText.Text = "Sin modelo 3D. Abre o crea un archivo .fcol, o añade colisiones para empezar.";
            EmptyViewText.Visibility = Visibility.Visible;
        }

        private void RenderViewport()
        {
            int width = Math.Max((int)OverlayCanvas.ActualWidth, 1);
            int height = Math.Max((int)OverlayCanvas.ActualHeight, 1);
            if (width < 2 || height < 2)
            {
                return;
            }

            byte[] pixels = RenderModel(width, height, out float[] depthBuffer);
            var bitmap = new WriteableBitmap(width, height, 96.0f, 96.0f, PixelFormats.Bgra32, null);
            bitmap.WritePixels(new Int32Rect(0, 0, width, height), pixels, width * 4, 0);
            ViewportImage.Source = bitmap;
            ViewportDepthBuffer = depthBuffer;
            ViewportPixelWidth = width;
            ViewportPixelHeight = height;
            UpdateCollisionOverlay(depthBuffer, width, height);
        }

        private byte[] RenderModel(int width, int height, out float[] depthBuffer)
        {
            int pixelCount = width * height;
            var pixels = new byte[pixelCount * 4];
            depthBuffer = new float[pixelCount];
            Array.Fill(depthBuffer, float.PositiveInfinity);
            for (int i = 0; i < pixelCount; ++i)
            {
                int offset = i * 4;
                pixels[offset + 0] = 0;
                pixels[offset + 1] = 0;
                pixels[offset + 2] = 0;
                pixels[offset + 3] = 255;
            }

            if (Model == null)
            {
                return pixels;
            }
            float focalLength = height * 0.5f / MathF.Tan(MathF.PI / 6.0f);
            foreach (var mesh in Model.Meshes)
            {
                foreach (var triangle in mesh.Triangles)
                {
                    if (triangle.V0 >= mesh.Vertices.Count || triangle.V1 >= mesh.Vertices.Count || triangle.V2 >= mesh.Vertices.Count)
                    {
                        continue;
                    }
                    var vertex0 = mesh.Vertices[(int)triangle.V0];
                    var vertex1 = mesh.Vertices[(int)triangle.V1];
                    var vertex2 = mesh.Vertices[(int)triangle.V2];
                    if (!ProjectPoint(vertex0.Position, width, height, focalLength, out var point0) ||
                        !ProjectPoint(vertex1.Position, width, height, focalLength, out var point1) ||
                        !ProjectPoint(vertex2.Position, width, height, focalLength, out var point2))
                    {
                        continue;
                    }
                    RasterizeTriangle(point0, point1, point2, width, height, depthBuffer);
                }
            }

            float nearestDepth = float.PositiveInfinity;
            float farthestDepth = 0.0f;
            foreach (float depth in depthBuffer)
            {
                if (!float.IsFinite(depth))
                {
                    continue;
                }
                nearestDepth = Math.Min(nearestDepth, depth);
                farthestDepth = Math.Max(farthestDepth, depth);
            }
            if (float.IsFinite(nearestDepth))
            {
                float depthRange = farthestDepth - nearestDepth;
                for (int i = 0; i < depthBuffer.Length; ++i)
                {
                    float depth = depthBuffer[i];
                    if (!float.IsFinite(depth))
                    {
                        continue;
                    }
                    float normalizedDepth = depthRange > 0.00001f
                        ? (depth - nearestDepth) / depthRange
                        : 0.0f;
                    byte intensity = ToByte(1.0f - normalizedDepth);
                    int offset = i * 4;
                    pixels[offset + 0] = intensity;
                    pixels[offset + 1] = intensity;
                    pixels[offset + 2] = intensity;
                }
            }
            return pixels;
        }

        private bool ProjectPoint(Vector3 position, int width, int height, float focalLength, out ProjectedVertex projected)
        {
            Vector3 relative = position - ModelCenter - PanOffset;
            GetCameraBasis(out Vector3 right, out Vector3 up, out Vector3 towardCamera);
            float depth = CameraDistance - Vector3.Dot(relative, towardCamera);
            if (depth <= 0.01f)
            {
                projected = default;
                return false;
            }
            float inverseDepth = focalLength / depth;
            float screenX = width * 0.5f + Vector3.Dot(relative, right) * inverseDepth;
            float screenY = height * 0.5f - Vector3.Dot(relative, up) * inverseDepth;
            if (!float.IsFinite(screenX) || !float.IsFinite(screenY))
            {
                projected = default;
                return false;
            }
            projected = new ProjectedVertex(
                new Point(screenX, screenY),
                depth);
            return true;
        }

        private static void RasterizeTriangle(ProjectedVertex a, ProjectedVertex b, ProjectedVertex c, int width, int height, float[] depthBuffer)
        {
            float area = Edge(a.Position, b.Position, (float)c.Position.X, (float)c.Position.Y);
            if (Math.Abs(area) < 0.00001f)
            {
                return;
            }
            float minimumX = (float)Math.Min(a.Position.X, Math.Min(b.Position.X, c.Position.X));
            float maximumX = (float)Math.Max(a.Position.X, Math.Max(b.Position.X, c.Position.X));
            float minimumY = (float)Math.Min(a.Position.Y, Math.Min(b.Position.Y, c.Position.Y));
            float maximumY = (float)Math.Max(a.Position.Y, Math.Max(b.Position.Y, c.Position.Y));
            if (maximumX < 0.0f || minimumX >= width || maximumY < 0.0f || minimumY >= height)
            {
                return;
            }
            int minX = (int)MathF.Floor(Math.Clamp(minimumX, 0.0f, width - 1.0f));
            int maxX = (int)MathF.Ceiling(Math.Clamp(maximumX, 0.0f, width - 1.0f));
            int minY = (int)MathF.Floor(Math.Clamp(minimumY, 0.0f, height - 1.0f));
            int maxY = (int)MathF.Ceiling(Math.Clamp(maximumY, 0.0f, height - 1.0f));
            for (int y = minY; y <= maxY; ++y)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    float sampleX = x + 0.5f;
                    float sampleY = y + 0.5f;
                    float weightA = Edge(b.Position, c.Position, sampleX, sampleY) / area;
                    float weightB = Edge(c.Position, a.Position, sampleX, sampleY) / area;
                    float weightC = Edge(a.Position, b.Position, sampleX, sampleY) / area;
                    if (weightA < 0.0f || weightB < 0.0f || weightC < 0.0f)
                    {
                        continue;
                    }

                    int pixelIndex = y * width + x;
                    float inverseDepth = weightA / a.Depth + weightB / b.Depth + weightC / c.Depth;
                    if (inverseDepth <= 0.0f)
                    {
                        continue;
                    }
                    float depth = 1.0f / inverseDepth;
                    if (depth >= depthBuffer[pixelIndex])
                    {
                        continue;
                    }
                    depthBuffer[pixelIndex] = depth;
                }
            }
        }

        private static float Edge(Point a, Point b, float x, float y)
        {
            return (x - (float)a.X) * ((float)b.Y - (float)a.Y) - (y - (float)a.Y) * ((float)b.X - (float)a.X);
        }

        private static byte ToByte(float value)
        {
            if (!float.IsFinite(value))
            {
                return 0;
            }
            return (byte)Math.Clamp((int)MathF.Round(value * 255.0f), 0, 255);
        }

        private void UpdateCollisionOverlay(float[]? depthBuffer = null, int width = 0, int height = 0)
        {
            OverlayCanvas.Children.Clear();
            depthBuffer ??= ViewportDepthBuffer;
            width = width > 0 ? width : ViewportPixelWidth;
            height = height > 0 ? height : ViewportPixelHeight;
            if (width < 1 || height < 1)
            {
                return;
            }
            float focalLength = height * 0.5f / MathF.Tan(MathF.PI / 6.0f);
            foreach (var collision in Collisions)
            {
                DrawCollision(collision, width, height, focalLength, depthBuffer);
            }
        }

        private void DrawCollision(CollisionVolume collision, int width, int height, float focalLength, float[]? depthBuffer)
        {
            Vector3 halfSize = collision.Size * 0.5f;
            Quaternion rotation = collision.Shape == CollisionShape.OBB
                ? Quaternion.CreateFromYawPitchRoll(ToRadians(collision.RotationY), ToRadians(collision.RotationX), ToRadians(collision.RotationZ))
                : Quaternion.Identity;
            var corners = new ProjectedVertex[8];
            var visible = new bool[8];
            for (int i = 0; i < corners.Length; ++i)
            {
                Vector3 local = new(
                    (i & 1) == 0 ? -halfSize.X : halfSize.X,
                    (i & 2) == 0 ? -halfSize.Y : halfSize.Y,
                    (i & 4) == 0 ? -halfSize.Z : halfSize.Z);
                Vector3 position = collision.Center + Vector3.Transform(local, rotation);
                visible[i] = ProjectPoint(position, width, height, focalLength, out corners[i]);
            }

            Brush color = collision.Kind == CollisionKind.Trigger
                ? new SolidColorBrush(Color.FromArgb(230, 63, 213, 201))
                : new SolidColorBrush(Color.FromArgb(230, 255, 112, 91));
            bool selected = ReferenceEquals(CollisionList.SelectedItem, collision);
            var edges = new (int A, int B)[]
            {
                (0, 1), (0, 2), (0, 4), (1, 3), (1, 5), (2, 3),
                (2, 6), (3, 7), (4, 5), (4, 6), (5, 7), (6, 7)
            };
            foreach (var edge in edges)
            {
                if (!visible[edge.A] || !visible[edge.B])
                {
                    continue;
                }
                DrawDepthTestedEdge(corners[edge.A], corners[edge.B], width, height, depthBuffer, selected ? Brushes.DodgerBlue : color, selected ? 2.5 : 1.5);
            }

            if (ProjectPoint(collision.Center, width, height, focalLength, out var labelPosition))
            {
                var label = new TextBlock
                {
                    Text = $"ID {collision.Id}",
                    Foreground = selected ? Brushes.DodgerBlue : color,
                    FontSize = 12,
                    FontWeight = FontWeights.SemiBold,
                    IsHitTestVisible = false
                };
                Canvas.SetLeft(label, labelPosition.Position.X + 5.0);
                Canvas.SetTop(label, labelPosition.Position.Y + 5.0);
                OverlayCanvas.Children.Add(label);
            }
        }

        private void DrawDepthTestedEdge(ProjectedVertex start, ProjectedVertex end, int width, int height, float[]? depthBuffer, Brush color, double thickness)
        {
            if (depthBuffer == null || depthBuffer.Length != width * height)
            {
                OverlayCanvas.Children.Add(new Line
                {
                    X1 = start.Position.X,
                    Y1 = start.Position.Y,
                    X2 = end.Position.X,
                    Y2 = end.Position.Y,
                    Stroke = color,
                    StrokeThickness = thickness,
                    IsHitTestVisible = false
                });
                return;
            }

            float deltaX = (float)(end.Position.X - start.Position.X);
            float deltaY = (float)(end.Position.Y - start.Position.Y);
            int steps = Math.Max(1, (int)MathF.Ceiling(Math.Max(Math.Abs(deltaX), Math.Abs(deltaY))));
            var visiblePoints = new PointCollection();
            for (int step = 0; step <= steps; ++step)
            {
                float blend = (float)step / steps;
                float x = (float)start.Position.X + deltaX * blend;
                float y = (float)start.Position.Y + deltaY * blend;
                float lineDepth = start.Depth + (end.Depth - start.Depth) * blend;
                int pixelX = (int)MathF.Round(x);
                int pixelY = (int)MathF.Round(y);
                bool inViewport = pixelX >= 0 && pixelX < width && pixelY >= 0 && pixelY < height;
                bool visible = inViewport && lineDepth <= depthBuffer[pixelY * width + pixelX] + Math.Max(0.002f, lineDepth * 0.001f);
                if (visible)
                {
                    visiblePoints.Add(new Point(x, y));
                }
                else
                {
                    AddVisiblePolyline(visiblePoints, color, thickness);
                    visiblePoints = new PointCollection();
                }
            }
            AddVisiblePolyline(visiblePoints, color, thickness);
        }

        private void AddVisiblePolyline(PointCollection points, Brush color, double thickness)
        {
            if (points.Count < 2)
            {
                return;
            }
            OverlayCanvas.Children.Add(new Polyline
            {
                Points = points,
                Stroke = color,
                StrokeThickness = thickness,
                IsHitTestVisible = false
            });
        }

        private static float ToRadians(float degrees)
        {
            return degrees * (MathF.PI / 180.0f);
        }

        private void GetCameraBasis(out Vector3 right, out Vector3 up, out Vector3 towardCamera)
        {
            float cosPitch = MathF.Cos(CameraPitch);
            towardCamera = Vector3.Normalize(new Vector3(
                cosPitch * MathF.Cos(CameraYaw),
                cosPitch * MathF.Sin(CameraYaw),
                MathF.Sin(CameraPitch)));
            Vector3 forward = -towardCamera;
            right = Vector3.Normalize(Vector3.Cross(forward, Vector3.UnitZ));
            up = Vector3.Normalize(Vector3.Cross(right, forward));
        }

        private void OverlayCanvas_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            RenderViewport();
        }

        private void OverlayCanvas_MouseDown(object sender, MouseButtonEventArgs e)
        {
            DragButton = e.ChangedButton;
            LastMousePosition = e.GetPosition(OverlayCanvas);
            OverlayCanvas.CaptureMouse();
            e.Handled = true;
        }

        private void OverlayCanvas_MouseMove(object sender, MouseEventArgs e)
        {
            if (!OverlayCanvas.IsMouseCaptured)
            {
                return;
            }
            Point current = e.GetPosition(OverlayCanvas);
            double deltaX = current.X - LastMousePosition.X;
            double deltaY = current.Y - LastMousePosition.Y;
            LastMousePosition = current;

            if (DragButton == MouseButton.Left)
            {
                CameraYaw += (float)deltaX * 0.01f;
                CameraPitch = Math.Clamp(CameraPitch + (float)deltaY * 0.01f, -1.5f, 1.5f);
            }
            else if (DragButton == MouseButton.Right || DragButton == MouseButton.Middle)
            {
                float unitsPerPixel = CameraDistance / Math.Max((float)OverlayCanvas.ActualHeight, 1.0f);
                GetCameraBasis(out Vector3 right, out Vector3 up, out _);
                PanOffset += right * (float)(-deltaX * unitsPerPixel) + up * (float)(deltaY * unitsPerPixel);
            }
            RenderViewport();
        }

        private void OverlayCanvas_MouseUp(object sender, MouseButtonEventArgs e)
        {
            if (OverlayCanvas.IsMouseCaptured)
            {
                OverlayCanvas.ReleaseMouseCapture();
            }
        }

        private void OverlayCanvas_MouseWheel(object sender, MouseWheelEventArgs e)
        {
            CameraDistance *= e.Delta > 0 ? 0.88f : 1.14f;
            float minimumCameraDistance = ModelRadius * 1.25f + 0.01f;
            CameraDistance = Math.Max(CameraDistance, minimumCameraDistance);
            RenderViewport();
            e.Handled = true;
        }

        private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (Keyboard.Modifiers == ModifierKeys.Control && e.Key == Key.S)
            {
                SaveCollisions();
                e.Handled = true;
            }
            else if (e.Key == Key.Delete && e.OriginalSource is not TextBox)
            {
                DeleteSelectedCollision();
                e.Handled = true;
            }
        }

        private void SetStatus(string message)
        {
            StatusText.Text = message;
        }

        private readonly record struct ProjectedVertex(Point Position, float Depth);
    }
}
