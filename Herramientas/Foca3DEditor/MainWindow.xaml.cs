using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Microsoft.Win32;
using Foca3DEditor.Models;

namespace Foca3DEditor
{
    public partial class MainWindow : Window
    {
        private List<Fc3dFile> _loadedModels = new();
        private FocaProject _project = new();
        private string? _currentProjectPath;

        private Point3D _cameraTarget = new Point3D(0, 0, 0);
        private double _cameraDistance = 5.0;
        private double _cameraYaw = 55.0;
        private double _cameraPitch = 20.0;
        private bool _modelFacesX = true;
        private Point _lastMousePos;
        private bool _isOrbiting = false;
        private bool _isPanning = false;

        public MainWindow()
        {
            InitializeComponent();
            UpdateCamera();
        }

        private void UpdateCamera()
        {
            double radYaw = _cameraYaw * Math.PI / 180.0;
            double radPitch = _cameraPitch * Math.PI / 180.0;

            double x = _cameraTarget.X + _cameraDistance * Math.Cos(radPitch) * Math.Sin(radYaw);
            double y = _cameraTarget.Y - _cameraDistance * Math.Cos(radPitch) * Math.Cos(radYaw);
            double z = _cameraTarget.Z + _cameraDistance * Math.Sin(radPitch);

            MainCamera.Position = new Point3D(x, y, z);
            MainCamera.LookDirection = new Vector3D(_cameraTarget.X - x, _cameraTarget.Y - y, _cameraTarget.Z - z);
            MainCamera.UpDirection = new Vector3D(0, 0, 1);
        }

        public void FocusOnModel()
        {
            if (_loadedModels.Count == 0)
            {
                _cameraTarget = new Point3D(0, 0, 0);
                _cameraDistance = 5.0;
                _cameraYaw = -45.0;
                _cameraPitch = 20.0;
                UpdateCamera();
                return;
            }

            double minX = double.MaxValue, minY = double.MaxValue, minZ = double.MaxValue;
            double maxX = double.MinValue, maxY = double.MinValue, maxZ = double.MinValue;
            bool hasVerts = false;

            foreach (var file in _loadedModels)
            {
                if (file == null) continue;
                foreach (var mesh in file.Meshes)
                {
                    foreach (var v in mesh.Vertices)
                    {
                        hasVerts = true;
                        if (v.Position.X < minX) minX = v.Position.X;
                        if (v.Position.X > maxX) maxX = v.Position.X;
                        if (v.Position.Y < minY) minY = v.Position.Y;
                        if (v.Position.Y > maxY) maxY = v.Position.Y;
                        if (v.Position.Z < minZ) minZ = v.Position.Z;
                        if (v.Position.Z > maxZ) maxZ = v.Position.Z;
                    }
                }
            }

            if (!hasVerts)
            {
                _cameraTarget = new Point3D(0, 0, 0);
                _cameraDistance = 5.0;
                _cameraYaw = -45.0;
                _cameraPitch = 20.0;
                UpdateCamera();
                return;
            }

            _cameraTarget = new Point3D((minX + maxX) * 0.5, (minY + maxY) * 0.5, (minZ + maxZ) * 0.5);
            double sizeX = maxX - minX;
            double sizeY = maxY - minY;
            double sizeZ = maxZ - minZ;
            double maxDim = Math.Max(sizeX, Math.Max(sizeY, sizeZ));
            if (maxDim < 0.01) maxDim = 1.0;

            _modelFacesX = sizeY >= sizeX;

            _cameraDistance = maxDim * 2.2;
            _cameraYaw = _modelFacesX ? 55.0 : -45.0;
            _cameraPitch = 20.0;
            UpdateCamera();
        }

        private void Viewport_MouseRightButtonDown(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            _lastMousePos = e.GetPosition((IInputElement)sender);
            if ((System.Windows.Input.Keyboard.Modifiers & System.Windows.Input.ModifierKeys.Shift) != 0)
            {
                _isPanning = true;
            }
            else
            {
                _isOrbiting = true;
            }
            ((UIElement)sender).CaptureMouse();
        }

        private void Viewport_MouseDown(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (e.MiddleButton == System.Windows.Input.MouseButtonState.Pressed)
            {
                _lastMousePos = e.GetPosition((IInputElement)sender);
                _isPanning = true;
                ((UIElement)sender).CaptureMouse();
            }
        }

        private void Viewport_MouseMove(object sender, System.Windows.Input.MouseEventArgs e)
        {
            if (!_isOrbiting && !_isPanning) return;

            Point currentPos = e.GetPosition((IInputElement)sender);
            double deltaX = currentPos.X - _lastMousePos.X;
            double deltaY = currentPos.Y - _lastMousePos.Y;
            _lastMousePos = currentPos;

            if (_isOrbiting && e.RightButton == System.Windows.Input.MouseButtonState.Pressed)
            {
                _cameraYaw -= deltaX * 0.4;
                _cameraPitch = Math.Clamp(_cameraPitch - deltaY * 0.4, -89.0, 89.0);
                UpdateCamera();
            }
            else if (_isPanning && (e.MiddleButton == System.Windows.Input.MouseButtonState.Pressed ||
                     (e.RightButton == System.Windows.Input.MouseButtonState.Pressed && (System.Windows.Input.Keyboard.Modifiers & System.Windows.Input.ModifierKeys.Shift) != 0)))
            {
                Vector3D look = MainCamera.LookDirection;
                look.Normalize();
                Vector3D right = Vector3D.CrossProduct(look, MainCamera.UpDirection);
                right.Normalize();
                Vector3D up = Vector3D.CrossProduct(right, look);
                up.Normalize();

                double panSpeed = _cameraDistance * 0.0015;
                _cameraTarget += right * (-deltaX * panSpeed) + up * (deltaY * panSpeed);
                UpdateCamera();
            }
        }

        private void Viewport_MouseRightButtonUp(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (_isOrbiting || _isPanning)
            {
                _isOrbiting = false;
                _isPanning = false;
                ((UIElement)sender).ReleaseMouseCapture();
            }
        }

        private void Viewport_MouseUp(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (_isPanning && e.MiddleButton == System.Windows.Input.MouseButtonState.Released)
            {
                _isPanning = false;
                ((UIElement)sender).ReleaseMouseCapture();
            }
        }

        private void Viewport_MouseWheel(object sender, System.Windows.Input.MouseWheelEventArgs e)
        {
            double zoomFactor = e.Delta > 0 ? 0.85 : 1.15;
            _cameraDistance = Math.Clamp(_cameraDistance * zoomFactor, 0.1, 5000.0);
            UpdateCamera();
        }

        private void Window_KeyDown(object sender, System.Windows.Input.KeyEventArgs e)
        {
            switch (e.Key)
            {
                case System.Windows.Input.Key.F:
                    FocusOnModel();
                    break;
                case System.Windows.Input.Key.D1:
                case System.Windows.Input.Key.NumPad1:
                    FrontView_Click(this, new RoutedEventArgs());
                    break;
                case System.Windows.Input.Key.D3:
                case System.Windows.Input.Key.NumPad3:
                    SideView_Click(this, new RoutedEventArgs());
                    break;
                case System.Windows.Input.Key.D7:
                case System.Windows.Input.Key.NumPad7:
                    TopView_Click(this, new RoutedEventArgs());
                    break;
                case System.Windows.Input.Key.D5:
                case System.Windows.Input.Key.NumPad5:
                    IsoView_Click(this, new RoutedEventArgs());
                    break;
            }
        }

        private void FocusView_Click(object sender, RoutedEventArgs e)
        {
            FocusOnModel();
        }

        private void FrontView_Click(object sender, RoutedEventArgs e)
        {
            _cameraYaw = _modelFacesX ? 90.0 : 0.0;
            _cameraPitch = 0.0;
            UpdateCamera();
        }

        private void BackView_Click(object sender, RoutedEventArgs e)
        {
            _cameraYaw = _modelFacesX ? -90.0 : 180.0;
            _cameraPitch = 0.0;
            UpdateCamera();
        }

        private void SideView_Click(object sender, RoutedEventArgs e)
        {
            _cameraYaw = _modelFacesX ? 0.0 : 90.0;
            _cameraPitch = 0.0;
            UpdateCamera();
        }

        private void LeftView_Click(object sender, RoutedEventArgs e)
        {
            _cameraYaw = _modelFacesX ? 180.0 : -90.0;
            _cameraPitch = 0.0;
            UpdateCamera();
        }

        private void TopView_Click(object sender, RoutedEventArgs e)
        {
            _cameraYaw = _modelFacesX ? 90.0 : 0.0;
            _cameraPitch = 89.9;
            UpdateCamera();
        }

        private void IsoView_Click(object sender, RoutedEventArgs e)
        {
            _cameraYaw = _modelFacesX ? 55.0 : -45.0;
            _cameraPitch = 20.0;
            UpdateCamera();
        }

        private void ToggleFrontAxis_Click(object sender, RoutedEventArgs e)
        {
            _modelFacesX = !_modelFacesX;
            FrontView_Click(sender, e);
        }

        private void NewProject_Click(object sender, RoutedEventArgs e)
        {
            _project = new FocaProject();
            _loadedModels.Clear();
            _currentProjectPath = null;
            Title = "Foca3D Editor";
            PopulateTree();
            Build3DView(true);
        }

        private void OpenProject_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "Foca Project (*.projfoca)|*.projfoca|All Files (*.*)|*.*",
                Title = "Open Project"
            };

            if (dialog.ShowDialog() == true)
            {
                try
                {
                    string json = File.ReadAllText(dialog.FileName);
                    var proj = JsonSerializer.Deserialize<FocaProject>(json);
                    if (proj != null)
                    {
                        _project = proj;
                        _currentProjectPath = dialog.FileName;
                        Title = $"Foca3D Editor - {Path.GetFileName(dialog.FileName)}";
                        
                        _loadedModels.Clear();
                        foreach(var mdl in _project.Models)
                        {
                            if (File.Exists(mdl)) _loadedModels.Add(Fc3dFile.Read(mdl));
                            else _loadedModels.Add(null);
                        }

                        PopulateTree();
                        Build3DView(true);
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error loading project: {ex.Message}");
                }
            }
        }

        private void SaveProject_Click(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrEmpty(_currentProjectPath))
            {
                SaveProjectAs_Click(sender, e);
                return;
            }
            SaveInternal(_currentProjectPath);
        }

        private void SaveProjectAs_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new SaveFileDialog
            {
                Filter = "Foca Project (*.projfoca)|*.projfoca",
                Title = "Save Project As"
            };

            if (dialog.ShowDialog() == true)
            {
                _currentProjectPath = dialog.FileName;
                SaveInternal(_currentProjectPath);
                Title = $"Foca3D Editor - {Path.GetFileName(dialog.FileName)}";
            }
        }

        private void SaveInternal(string path)
        {
            try
            {
                var options = new JsonSerializerOptions { WriteIndented = true };
                string json = JsonSerializer.Serialize(_project, options);
                File.WriteAllText(path, json);
                MessageBox.Show("Proyecto guardado correctamente.");
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error saving project: {ex.Message}");
            }
        }

        private void Export_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new SaveFileDialog
            {
                Filter = "Foca Format (*.foca)|*.foca",
                Title = "Export"
            };

            if (dialog.ShowDialog() == true)
            {
                try
                {
                    var exporter = new Foca3DEditor.Exporter.Exporter();
                    exporter.Export(dialog.FileName, _project, _loadedModels);
                    MessageBox.Show("Exportado correctamente.", "Export", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error al exportar: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void Abrir_Click(object sender, RoutedEventArgs e)
        {
            MdlMenuItem_Click(sender, e);
        }

        private void MdlMenuItem_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog
            {
                Filter = "F3D Files (*.f3d)|*.f3d|All Files (*.*)|*.*",
                Title = "Import F3D",
                Multiselect = true
            };

            if (dialog.ShowDialog() == true)
            {
                try
                {
                    foreach (string ruta in dialog.FileNames)
                    {
                        string nombre = System.IO.Path.GetFileName(ruta);
                        _project.Models.RemoveAll(m => System.IO.Path.GetFileName(m).Equals(nombre, StringComparison.OrdinalIgnoreCase));
                        _project.Models.Add(ruta);
                    }
                    
                    _loadedModels.Clear();
                    foreach (var mdl in _project.Models)
                    {
                        if (File.Exists(mdl)) _loadedModels.Add(Fc3dFile.Read(mdl));
                        else _loadedModels.Add(null);
                    }
                    
                    PopulateTree();
                    Build3DView(true);
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error al leer F3D: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void Salir_Click(object sender, RoutedEventArgs e)
        {
            Application.Current.Shutdown();
        }

        private void TxtMenuItem_Click(object sender, RoutedEventArgs e)
        {
            var openFileDialog = new OpenFileDialog
            {
                Filter = "Archivos de textura (*.tga;*.bmp;*.ctxe;*.a4;*.a8;*.l4;*.l8;*.la4;*.la8;*.ctx;*.rgb)|*.tga;*.bmp;*.ctxe;*.a4;*.a8;*.l4;*.l8;*.la4;*.la8;*.ctx;*.rgb",
                Title = "Añadir texturas",
                Multiselect = true
            };

            if (openFileDialog.ShowDialog() == true)
            {
                foreach (string ruta in openFileDialog.FileNames)
                {
                    string nombre = System.IO.Path.GetFileName(ruta);
                    string extension = System.IO.Path.GetExtension(ruta).ToLower();
                    string formato = extension.TrimStart('.');
                    var preview = TexturePreviewHelper.LoadTexture(ruta);
                    uint textureWidth = preview != null ? (uint)preview.PixelWidth : 0;
                    uint textureHeight = preview != null ? (uint)preview.PixelHeight : 0;
                    
                    var existing = _project.Textures.FirstOrDefault(t => string.Equals(t.Name, nombre, StringComparison.OrdinalIgnoreCase));
                    if (existing != null)
                    {
                        existing.Format = formato;
                        existing.SourcePath = ruta;
                        existing.Width = textureWidth;
                        existing.Height = textureHeight;
                    }
                    else
                    {
                        var entry = new TextureEntry
                        {
                            Name = nombre,
                            Format = formato,
                            SourcePath = ruta,
                            Width = textureWidth,
                            Height = textureHeight
                        };
                        _project.Textures.Add(entry);
                    }
                }
                PopulateTree();
            }
        }

        private void PopulateTree()
        {
            ModelsFolder.Items.Clear();
            TexturesFolder.Items.Clear();

            for (int i = 0; i < _project.Models.Count; i++)
            {
                string modelPath = _project.Models[i];
                string name = System.IO.Path.GetFileName(modelPath);
                Fc3dFile loadedModel = i < _loadedModels.Count ? _loadedModels[i] : null;

                var item = new TreeViewItem { Header = name, Tag = loadedModel };
                
                var contextMenu = new ContextMenu();
                var deleteItem = new MenuItem { Header = "Eliminar modelo" };
                
                // Create local copy of index for closure
                int idx = i;
                deleteItem.Click += (s, e) => {
                    _project.Models.RemoveAt(idx);
                    _loadedModels.RemoveAt(idx);
                    PopulateTree();
                    Build3DView(true);
                };
                contextMenu.Items.Add(deleteItem);
                item.ContextMenu = contextMenu;

                ModelsFolder.Items.Add(item);
            }

            foreach (var tex in _project.Textures)
            {
                var item = new TreeViewItem { Header = tex.Name, Tag = tex };
                
                var contextMenu = new ContextMenu();
                var deleteItem = new MenuItem { Header = "Eliminar textura" };
                deleteItem.Click += (s, e) => {
                    _project.Textures.Remove(tex);
                    PopulateTree();
                };
                contextMenu.Items.Add(deleteItem);
                item.ContextMenu = contextMenu;

                TexturesFolder.Items.Add(item);
            }

            UpdateFolderHeaders();
        }

        private void UpdateFolderHeaders()
        {
            ModelsFolder.Header = $"Model ({ModelsFolder.Items.Count})";
            TexturesFolder.Header = $"Texture ({TexturesFolder.Items.Count})";
        }

        private void Build3DView(bool refitCamera = false)
        {
            ModelGroup.Children.Clear();
            ModelGroup.Children.Add(new DirectionalLight(Colors.White, new Vector3D(-1, 1, -1)));
            ModelGroup.Children.Add(new DirectionalLight(Color.FromRgb(160, 160, 160), new Vector3D(1, -1, 1)));
            ModelGroup.Children.Add(new AmbientLight(Color.FromRgb(120, 120, 120)));

            for (int modelIdx = 0; modelIdx < _loadedModels.Count; modelIdx++)
            {
                var currentFile = _loadedModels[modelIdx];
                if (currentFile == null) continue;

                string modelFileName = modelIdx < _project.Models.Count ? Path.GetFileName(_project.Models[modelIdx]) : "";

                foreach (var mesh in currentFile.Meshes)
                {
                    // Check if material has an assigned texture
                    string assignedTexName = "";
                    Color defaultMatColor = Colors.LightGray;

                    if (mesh.MaterialIndex >= 0 && mesh.MaterialIndex < currentFile.Materials.Count)
                    {
                        var mat = currentFile.Materials[mesh.MaterialIndex];
                        string key = $"{modelFileName}_{mat.Name}";
                        if (_project.MaterialAssignments.ContainsKey(key))
                        {
                            assignedTexName = _project.MaterialAssignments[key];
                        }

                        defaultMatColor = Color.FromArgb(
                            (byte)Math.Clamp((int)(mat.Color[3] * 255), 0, 255),
                            (byte)Math.Clamp((int)(mat.Color[0] * 255), 0, 255),
                            (byte)Math.Clamp((int)(mat.Color[1] * 255), 0, 255),
                            (byte)Math.Clamp((int)(mat.Color[2] * 255), 0, 255)
                        );
                    }

                    ImageBrush? textureBrush = null;
                    if (!string.IsNullOrEmpty(assignedTexName))
                    {
                        var tex = _project.Textures.Find(t => string.Equals(t.Name, assignedTexName, StringComparison.OrdinalIgnoreCase));
                        if (tex != null && File.Exists(tex.SourcePath))
                        {
                            var bmp = TexturePreviewHelper.LoadTexture(tex.SourcePath);
                            if (bmp != null)
                            {
                                textureBrush = new ImageBrush(bmp);
                            }
                        }
                    }

                    // If a texture is assigned, render the whole mesh with that texture
                    if (textureBrush != null)
                    {
                        var geo = new MeshGeometry3D();
                        foreach (var v in mesh.Vertices)
                        {
                            geo.Positions.Add(new Point3D(v.Position.X, v.Position.Y, v.Position.Z));
                            geo.Normals.Add(new Vector3D(v.Normal.X, v.Normal.Y, v.Normal.Z));
                            if (v.UvLayers.Count > 0)
                                geo.TextureCoordinates.Add(new Point(v.UvLayers[0].X, 1.0 - v.UvLayers[0].Y));
                        }
                        foreach (var t in mesh.Triangles)
                        {
                            geo.TriangleIndices.Add((int)t.V0);
                            geo.TriangleIndices.Add((int)t.V1);
                            geo.TriangleIndices.Add((int)t.V2);
                        }

                        var mat = new DiffuseMaterial(textureBrush);
                        var geomModel = new GeometryModel3D(geo, mat) { BackMaterial = mat };
                        ModelGroup.Children.Add(geomModel);
                    }
                    else
                    {
                        // Group triangles by color (Vertex Color or Material Color)
                        var colorGroups = new Dictionary<int, List<Fc3dTriangle>>();

                        foreach (var t in mesh.Triangles)
                        {
                            var v0 = mesh.Vertices[(int)t.V0];
                            var v1 = mesh.Vertices[(int)t.V1];
                            var v2 = mesh.Vertices[(int)t.V2];

                            float r = (v0.Color.X + v1.Color.X + v2.Color.X) / 3f;
                            float g = (v0.Color.Y + v1.Color.Y + v2.Color.Y) / 3f;
                            float b = (v0.Color.Z + v1.Color.Z + v2.Color.Z) / 3f;
                            float a = (v0.Color.W + v1.Color.W + v2.Color.W) / 3f;

                            Color triColor;
                            bool hasVertexColor = Math.Abs(r - 1.0f) > 0.05f || Math.Abs(g - 1.0f) > 0.05f || Math.Abs(b - 1.0f) > 0.05f;
                            if (hasVertexColor)
                            {
                                triColor = Color.FromArgb(
                                    (byte)Math.Clamp((int)(a * 255), 0, 255),
                                    (byte)Math.Clamp((int)(r * 255), 0, 255),
                                    (byte)Math.Clamp((int)(g * 255), 0, 255),
                                    (byte)Math.Clamp((int)(b * 255), 0, 255)
                                );
                            }
                            else
                            {
                                triColor = defaultMatColor;
                            }

                            int key = (triColor.A << 24) | (triColor.R << 16) | (triColor.G << 8) | triColor.B;
                            if (!colorGroups.ContainsKey(key))
                                colorGroups[key] = new List<Fc3dTriangle>();
                            colorGroups[key].Add(t);
                        }

                        foreach (var kvp in colorGroups)
                        {
                            int key = kvp.Key;
                            var groupTriangles = kvp.Value;
                            Color groupColor = Color.FromArgb((byte)(key >> 24), (byte)((key >> 16) & 0xFF), (byte)((key >> 8) & 0xFF), (byte)(key & 0xFF));

                            var geo = new MeshGeometry3D();
                            int vIdx = 0;
                            foreach (var t in groupTriangles)
                            {
                                var v0 = mesh.Vertices[(int)t.V0];
                                var v1 = mesh.Vertices[(int)t.V1];
                                var v2 = mesh.Vertices[(int)t.V2];

                                geo.Positions.Add(new Point3D(v0.Position.X, v0.Position.Y, v0.Position.Z));
                                geo.Positions.Add(new Point3D(v1.Position.X, v1.Position.Y, v1.Position.Z));
                                geo.Positions.Add(new Point3D(v2.Position.X, v2.Position.Y, v2.Position.Z));

                                geo.Normals.Add(new Vector3D(v0.Normal.X, v0.Normal.Y, v0.Normal.Z));
                                geo.Normals.Add(new Vector3D(v1.Normal.X, v1.Normal.Y, v1.Normal.Z));
                                geo.Normals.Add(new Vector3D(v2.Normal.X, v2.Normal.Y, v2.Normal.Z));

                                geo.TriangleIndices.Add(vIdx);
                                geo.TriangleIndices.Add(vIdx + 1);
                                geo.TriangleIndices.Add(vIdx + 2);
                                vIdx += 3;
                            }

                            var brush = new SolidColorBrush(groupColor);
                            var mat = new DiffuseMaterial(brush);
                            var geomModel = new GeometryModel3D(geo, mat) { BackMaterial = mat };
                            ModelGroup.Children.Add(geomModel);
                        }
                    }
                }
            }

            if (refitCamera)
            {
                FocusOnModel();
            }
        }

        private void MainTreeView_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
        {
            MaterialsPanel.Children.Clear();
            if (e.NewValue is TreeViewItem item && item.Tag != null)
            {
                object data = item.Tag;
                if (data is Fc3dFile file)
                {
                    string modelFileName = item.Header.ToString();
                    MaterialsPanel.Children.Add(new TextBlock { Text = $"Modelo: {modelFileName}", FontWeight = FontWeights.Bold, Margin = new Thickness(0, 0, 0, 4) });
                    
                    var cbInclude = new CheckBox { Content = "Incluir datos en el archivo .foca", Margin = new Thickness(0, 5, 0, 10) };
                    cbInclude.IsChecked = !_project.ModelIncludeData.ContainsKey(modelFileName) || _project.ModelIncludeData[modelFileName];
                    cbInclude.Checked += (s, ev) => _project.ModelIncludeData[modelFileName] = true;
                    cbInclude.Unchecked += (s, ev) => _project.ModelIncludeData[modelFileName] = false;
                    MaterialsPanel.Children.Add(cbInclude);

                    MaterialsPanel.Children.Add(new TextBlock { Text = "Materiales", FontWeight = FontWeights.Bold, Margin = new Thickness(0, 5, 0, 4) });

                    for (int i = 0; i < file.Materials.Count; i++)
                    {
                        var mat = file.Materials[i];
                        var sp = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 2, 0, 2) };
                        sp.Children.Add(new TextBlock { Text = mat.Name, Width = 120, VerticalAlignment = VerticalAlignment.Center });
                        
                        var cb = new ComboBox { Width = 150 };
                        cb.Items.Add("Ninguna");
                        foreach (var t in _project.Textures) cb.Items.Add(t.Name);
                        
                        string key = $"{modelFileName}_{mat.Name}";
                        if (_project.MaterialAssignments.ContainsKey(key))
                            cb.SelectedItem = _project.MaterialAssignments[key];
                        else
                            cb.SelectedIndex = 0;

                        cb.SelectionChanged += (s, ev) => {
                            if (cb.SelectedIndex > 0 && cb.SelectedItem != null)
                                _project.MaterialAssignments[key] = cb.SelectedItem.ToString();
                            else
                                _project.MaterialAssignments.Remove(key);
                            
                            Build3DView();
                        };
                        
                        sp.Children.Add(cb);
                        MaterialsPanel.Children.Add(sp);
                    }
                }
                else if (data is TextureEntry tex)
                {
                    MaterialsPanel.Children.Add(new TextBlock { Text = $"Textura seleccionada: {tex.Name}", FontWeight = FontWeights.Bold, Margin = new Thickness(0, 0, 0, 10) });
                    
                    var cbInclude = new CheckBox { Content = "Incluir datos en el archivo .foca", Margin = new Thickness(0, 0, 0, 5) };
                    cbInclude.IsChecked = tex.IncludeData;
                    cbInclude.Checked += (s, ev) => tex.IncludeData = true;
                    cbInclude.Unchecked += (s, ev) => tex.IncludeData = false;
                    MaterialsPanel.Children.Add(cbInclude);
                }
            }
            else
            {
                MaterialsPanel.Children.Add(new TextBlock { Text = "Selecciona un elemento para ver sus opciones", Foreground = Brushes.Gray, Padding = new Thickness(4), HorizontalAlignment = HorizontalAlignment.Center });
            }
        }
        private void MainTreeView_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (MainTreeView.SelectedItem is TreeViewItem item && item.Tag is TextureEntry tex)
            {
                MostrarTextura(tex);
            }
        }

        private void MostrarTextura(TextureEntry tex)
        {
            try
            {
                var window = new Window
                {
                    Title = $"Vista Previa: {tex.Name}",
                    Width = 400,
                    Height = 400,
                    WindowStartupLocation = WindowStartupLocation.CenterOwner,
                    Owner = this
                };

                var bmp = TexturePreviewHelper.LoadTexture(tex.SourcePath);
                if (bmp != null)
                {
                    window.Content = new Image { Source = bmp, Stretch = Stretch.Uniform };
                }
                else
                {
                    window.Content = new TextBlock
                    {
                        Text = $"No se pudo previsualizar la textura nativamente.\n\nRuta: {tex.SourcePath}",
                        TextWrapping = TextWrapping.Wrap,
                        HorizontalAlignment = HorizontalAlignment.Center,
                        VerticalAlignment = VerticalAlignment.Center,
                        Foreground = Brushes.Gray
                    };
                }

                window.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error al intentar mostrar la textura: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
    }
}
