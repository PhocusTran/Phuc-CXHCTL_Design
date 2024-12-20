namespace Basler_Test
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        private System.Windows.Forms.Button btnExit;
        private System.Windows.Forms.Button buttonUpdatePPI;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button buttonUpdateThreshold;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.TextBox textBoxPixelPerInchCircle;
        private System.Windows.Forms.TextBox textBoxThreshold;
        private System.Windows.Forms.PictureBox pictureBox1;
    }
}

