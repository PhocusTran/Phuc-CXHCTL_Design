using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Linq;
using System.Threading;
using System.Windows.Forms;
using System.Timers;
using System.Runtime.InteropServices;
using Basler.Pylon;
using Emgu.CV.CvEnum;
using Emgu.CV;
using Emgu.CV.Structure;
using Emgu.CV.Util;
using System.Linq.Expressions;

using Emgu.CV.Reg;
using static System.Net.Mime.MediaTypeNames;

namespace Basler_Test
{
    public partial class Form1 : Form
    {
        private System.Timers.Timer timer;
        private Camera camera = null;

        // Biến toàn cục để lưu trữ giá trị pixel_per_inch và ngưỡng
        private double pixelPerInchCircle = 1670.418604651163;
        private double pixelPerInchLine;
        private double thresholdValue = 54.0;

        private Point center1;
        private Point center2;

        public Form1()
        {
            InitializeComponent();

            camera = new Camera();
            camera.CameraOpened += Configuration.AcquireContinuous;
            camera.ConnectionLost += OnConnectionLost;
            pixelPerInchLine = pixelPerInchCircle / 1.044045085410249;

            try
            {
                // Tìm kiếm và mở camera đầu tiên được tìm thấy
                camera.Open();
                camera.Parameters[PLCamera.ExposureAuto].SetValue(PLCamera.ExposureAuto.Off);

                // Thiết lập thời gian phơi sáng mong muốn (đơn vị: micro giây)
                camera.Parameters[PLCamera.ExposureTimeAbs].SetValue(100000);

                // Bắt đầu lấy ảnh
                camera.StreamGrabber.Start();
            }
            catch (Exception ex)
            {
                MessageBox.Show("Không thể kết nối đến camera: " + ex.Message);
                return;
            }

            // Bắt đầu timer để lấy ảnh liên tục
            timer = new System.Timers.Timer();
            timer.Interval = 60; // Khoảng thời gian giữa mỗi lần lấy ảnh (ms)
            timer.Elapsed += TimerElapsed;
            timer.Start();
        }

        private void TimerElapsed(object sender, ElapsedEventArgs e)
        {
            try
            {
                // Lấy ảnh từ camera
                using (IGrabResult grabResult = camera.StreamGrabber.RetrieveResult(5000, TimeoutHandling.ThrowException))
                {
                    if (grabResult.GrabSucceeded)
                    {
                        // Chuyển đổi ảnh từ camera Basler sang Bitmap
                        using (var bmp = new Bitmap(grabResult.Width, grabResult.Height, PixelFormat.Format32bppRgb))
                        {
                            var data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadWrite, PixelFormat.Format32bppRgb);

                            if (grabResult.PixelData is byte[] pixelData)
                            {
                                Marshal.Copy(pixelData, 0, data.Scan0, pixelData.Length);
                            }
                            else
                            {
                                throw new InvalidOperationException("Can not grab image");
                            }

                            bmp.UnlockBits(data);

                            // Tìm biên, vẽ biên và tính kích thước
                            Image<Bgr, byte> img = bmp.ToImage<Bgr, byte>();
                            Image<Gray, byte> grayImage = img.Convert<Gray, byte>();
                            Image<Gray, byte> blur = grayImage.SmoothGaussian(3); // ***
                            Image<Gray, byte> thresh = blur.ThresholdBinary(new Gray(thresholdValue), new Gray(105));

                            Mat kernel = CvInvoke.GetStructuringElement(Emgu.CV.CvEnum.ElementShape.Rectangle, new Size(3, 3), new Point(-1, -1));
                            Image<Gray, byte> opening = thresh.MorphologyEx(Emgu.CV.CvEnum.MorphOp.Open, kernel, new Point(-1, -1), 1, Emgu.CV.CvEnum.BorderType.Default, new MCvScalar()); // ***
                            VectorOfVectorOfPoint contours = new VectorOfVectorOfPoint();
                            Mat hierarchy = new Mat();

                            CvInvoke.FindContours(opening, contours, hierarchy, RetrType.External, ChainApproxMethod.ChainApproxSimple);

                            // Lọc contours theo diện tích
                            VectorOfVectorOfPoint approx = new VectorOfVectorOfPoint();
                            Dictionary<int, double> shapes = new Dictionary<int, double>();

                            for (int i = 0; i < contours.Size; i++)
                            {
                                approx.Clear();
                                double perimeter = CvInvoke.ArcLength(contours[i], true);
                                CvInvoke.ApproxPolyDP(contours[i], approx, 0.04 * perimeter, true);
                                double area = CvInvoke.ContourArea(contours[i]);
                                if (approx.Length > 6)
                                {
                                    shapes.Add(i, area);
                                }
                            }

                            // Lưu trữ dữ liệu hình tròn
                            var circleData = new Dictionary<Point, double>();
                            var circleCount = 0;

                            if (shapes.Count > 0)
                            {
                                var sortedShapes = (from item in shapes
                                                    orderby item.Value ascending
                                                    select item).ToList();

                                for (int i = 0; i < sortedShapes.Count; i++)
                                {
                                    var moments = CvInvoke.Moments(contours[sortedShapes[i].Key]);
                                    // Tìm kiếm hình chữ nhật bao quanh đường viền
                                    var rect = CvInvoke.BoundingRectangle(contours[sortedShapes[i].Key]); // Sửa lỗi

                                    var cX = (int)(moments.M10 / moments.M00);
                                    var cY = (int)(moments.M01 / moments.M00);
                                    var position = new Point(cX, cY);

                                    // Lưu trữ giá trị của hình tròn
                                    if (circleCount == 0)
                                    {
                                        center1 = position;
                                        circleCount++;
                                        Console.WriteLine($"Hình tròn 1: x1={rect.X}, y1={rect.Y}, w1={rect.Width}, h1={rect.Height}, center1={center1}");
                                    }
                                    else if (circleCount == 1)
                                    {
                                        center2 = position;
                                        circleCount++;
                                        Console.WriteLine($"Hình tròn 2: x2={rect.X}, y2={rect.Y}, w2={rect.Width}, h2={rect.Height}, center2={center2}");
                                    }

                                    // Vẽ đường viền và tâm hình tròn
                                    CvInvoke.Rectangle(img, rect, new MCvScalar(0, 0, 255));
                                    CvInvoke.DrawContours(img, contours, sortedShapes[i].Key, new MCvScalar(0, 0, 255), 2);
                                    CvInvoke.Circle(img, position, 15, new MCvScalar(0, 255, 0));

                                    // Vẽ đường kính và thông tin đường kính
                                    CvInvoke.Line(img, new Point(rect.X, rect.Y + rect.Height / 2), new Point(rect.X + rect.Width, rect.Y + rect.Height / 2), new MCvScalar(255, 255, 0), 3);
                                    var diameterMm = (rect.Width * 25.4) / pixelPerInchCircle;
                                    CvInvoke.PutText(img, $"Diameter: {diameterMm:F3}mm", new Point(cX - 50, cY - 50), FontFace.HersheySimplex, 2, new MCvScalar(255, 255, 0), 3);
                                    // PutText(img, "content", position, fontStyle, fontScale, color, thickness)

                                    // Vẽ nhãn lên ảnh
                                    CvInvoke.PutText(img, $"({cX}, {cY})", new Point(cX - 200, cY + 100), FontFace.HersheySimplex, 2, new MCvScalar(255, 255, 0), 3);

                                    // Lưu đường kính vào dictionary
                                    circleData[position] = diameterMm;

                                    Console.WriteLine($"Đường kính của hình tròn tọa độ {position} là: {diameterMm:F3}mm");

                                    // Vẽ đường thẳng giữa tâm của hai hình tròn
                                    if (center1 != null && center2 != null)
                                    {
                                        // Tính phương trình đường thẳng
                                        var x1 = center1.X;
                                        var y1 = center1.Y;
                                        var x2 = center2.X;
                                        var y2 = center2.Y;

                                        // Chọn hai điểm trên đường thẳng vuông góc
                                        var point1_c1 = center1; // Điểm đầu tiên trùng với tâm của hình tròn thứ nhất
                                        var point2_c1 = new Point(x1 - (x1 - x2), y1); // Chọn điểm thứ 2 cách điểm đầu tiên 100 pixel

                                        var point1_c2 = center2;
                                        var point2_c2 = new Point(x1, y2 + (y1 - y2));

                                        // Vẽ đường thẳng
                                        CvInvoke.Line(img, point1_c1, point2_c1, new MCvScalar(255, 0, 0), 3);
                                        CvInvoke.Line(img, point1_c2, point2_c2, new MCvScalar(0, 255, 0), 3);

                                        // Tính độ dài của hai đường thẳng
                                        var lengthLine1 = Math.Sqrt(Math.Pow(point2_c1.X - x1, 2) + Math.Pow(point2_c1.Y - y1, 2));
                                        var lengthLine2 = Math.Sqrt(Math.Pow(point2_c2.X - x2, 2) + Math.Pow(point2_c2.Y - y2, 2));

                                        var lengthLine1Mm = lengthLine1 * 25.4 / pixelPerInchLine;
                                        var lengthLine2Mm = lengthLine2 * 25.4 / pixelPerInchLine;

                                        CvInvoke.PutText(img, $"{lengthLine1Mm:F3}mm", new Point(x1 - 1100, y1 - 50), FontFace.HersheySimplex, 2, new MCvScalar(255, 255, 0), 3);
                                        CvInvoke.PutText(img, $"{lengthLine2Mm:F3}mm", new Point(x2 + 50, y2 + 1000), FontFace.HersheySimplex, 2, new MCvScalar(255, 255, 0), 3);
                                    }
                                }
                            }

                            pictureBox1.Invoke((MethodInvoker)delegate
                            {
                                pictureBox1.Image?.Dispose(); // Giải phóng ảnh cũ trước khi hiển thị ảnh mới
                                pictureBox1.Image = img.ToBitmap();
                            });
                        }
                    }
                    else
                    {
                        // Xử lý lỗi khi không lấy được ảnh thành công
                        Console.WriteLine($"Lỗi khi lấy ảnh: {grabResult.ErrorCode} - {grabResult.ErrorDescription}");
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Lỗi khi lấy ảnh: {ex.Message}");
            }
        }

        private void OnConnectionLost(object sender, EventArgs e)
        {
            MessageBox.Show("Mất kết nối đến camera.");
            timer.Stop();
            camera.Close();
        }

        // Hàm cập nhật giá trị pixel_per_inch_line
        private void UpdatePixelPerInchLine()
        {
            pixelPerInchLine = pixelPerInchCircle / 1.044045085410249;
        }

        private void UpdatePixelPerInchCircle(object sender, EventArgs e)
        {
            pixelPerInchCircle = double.Parse(textBoxPixelPerInchCircle.Text);
            textBoxPixelPerInchCircle.Text = pixelPerInchCircle.ToString();
        }

        private void UpdateThreshold(object sender, EventArgs e)
        {
            thresholdValue = int.Parse(textBoxThreshold.Text);
            textBoxThreshold.Text = thresholdValue.ToString();
        }

        private void buttonUpdatePPI_Click(object sender, EventArgs e)
        {
            UpdatePixelPerInchCircle(sender, e);
        }

        private void buttonUpdateThreshold_Click(object sender, EventArgs e)
        {
            UpdateThreshold(sender, e);
        }

        private void pictureBox1_Click(object sender, EventArgs e)
        {

        }

        private void InitializeComponent()
        {
            this.buttonUpdatePPI = new System.Windows.Forms.Button();
            this.label1 = new System.Windows.Forms.Label();
            this.buttonUpdateThreshold = new System.Windows.Forms.Button();
            this.label2 = new System.Windows.Forms.Label();
            this.textBoxPixelPerInchCircle = new System.Windows.Forms.TextBox();
            this.textBoxThreshold = new System.Windows.Forms.TextBox();
            this.pictureBox1 = new System.Windows.Forms.PictureBox();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).BeginInit();
            this.SuspendLayout();
            // 
            // buttonUpdatePPI
            // 
            this.buttonUpdatePPI.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.buttonUpdatePPI.Location = new System.Drawing.Point(800, 30);
            this.buttonUpdatePPI.Name = "buttonUpdatePPI";
            this.buttonUpdatePPI.Size = new System.Drawing.Size(118, 30);
            this.buttonUpdatePPI.TabIndex = 0;
            this.buttonUpdatePPI.Text = "Update PPI";
            this.buttonUpdatePPI.UseVisualStyleBackColor = true;
            this.buttonUpdatePPI.Click += new System.EventHandler(this.buttonUpdatePPI_Click);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label1.Location = new System.Drawing.Point(589, 37);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(32, 17);
            this.label1.TabIndex = 1;
            this.label1.Text = "PPI";
            // 
            // buttonUpdateThreshold
            // 
            this.buttonUpdateThreshold.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.buttonUpdateThreshold.Location = new System.Drawing.Point(800, 66);
            this.buttonUpdateThreshold.Name = "buttonUpdateThreshold";
            this.buttonUpdateThreshold.Size = new System.Drawing.Size(118, 30);
            this.buttonUpdateThreshold.TabIndex = 3;
            this.buttonUpdateThreshold.Text = "Update Threshold";
            this.buttonUpdateThreshold.UseVisualStyleBackColor = true;
            this.buttonUpdateThreshold.Click += new System.EventHandler(this.buttonUpdateThreshold_Click);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label2.Location = new System.Drawing.Point(540, 72);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(81, 17);
            this.label2.TabIndex = 4;
            this.label2.Text = "Threshold";
            // 
            // textBoxPixelPerInchCircle
            // 
            this.textBoxPixelPerInchCircle.Location = new System.Drawing.Point(627, 36);
            this.textBoxPixelPerInchCircle.Name = "textBoxPixelPerInchCircle";
            this.textBoxPixelPerInchCircle.Size = new System.Drawing.Size(148, 20);
            this.textBoxPixelPerInchCircle.TabIndex = 0;
            // 
            // textBoxThreshold
            // 
            this.textBoxThreshold.Location = new System.Drawing.Point(627, 71);
            this.textBoxThreshold.Name = "textBoxThreshold";
            this.textBoxThreshold.Size = new System.Drawing.Size(148, 20);
            this.textBoxThreshold.TabIndex = 5;
            // 
            // pictureBox1
            // 
            this.pictureBox1.Location = new System.Drawing.Point(1, 97);
            this.pictureBox1.Name = "pictureBox1";
            this.pictureBox1.Size = new System.Drawing.Size(1474, 807);
            this.pictureBox1.TabIndex = 2;
            this.pictureBox1.TabStop = false;
            this.pictureBox1.Click += new System.EventHandler(this.pictureBox1_Click);
            // 
            // Form1
            // 
            this.ClientSize = new System.Drawing.Size(1487, 904);
            this.Controls.Add(this.textBoxThreshold);
            this.Controls.Add(this.textBoxPixelPerInchCircle);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.buttonUpdateThreshold);
            this.Controls.Add(this.pictureBox1);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.buttonUpdatePPI);
            this.Name = "Form1";
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        private void Form1_Load(object sender, EventArgs e)
        {
            textBoxPixelPerInchCircle.Text = pixelPerInchCircle.ToString();
            textBoxThreshold.Text = thresholdValue.ToString();
        }
    }
}