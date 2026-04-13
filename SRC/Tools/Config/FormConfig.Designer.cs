using System.Windows.Forms;

namespace Metin2Config
{
    partial class FormConfig
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
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

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(FormConfig));
            this.btnOK = new System.Windows.Forms.Button();
            this.btnCANCEL = new System.Windows.Forms.Button();
            this.gboxDisplaySettings = new System.Windows.Forms.GroupBox();
            this.cboxGamma = new System.Windows.Forms.ComboBox();
            this.cboxFrequency = new System.Windows.Forms.ComboBox();
            this.cboxResolution = new System.Windows.Forms.ComboBox();
            this.lblGamma = new System.Windows.Forms.Label();
            this.lblFrequency = new System.Windows.Forms.Label();
            this.lblResolution = new System.Windows.Forms.Label();
            this.gboxSoundSettings = new System.Windows.Forms.GroupBox();
            this.trbarSFX = new System.Windows.Forms.TrackBar();
            this.trbarBGM = new System.Windows.Forms.TrackBar();
            this.tboxSFX = new System.Windows.Forms.TextBox();
            this.tboxBGM = new System.Windows.Forms.TextBox();
            this.groupBox6 = new System.Windows.Forms.GroupBox();
            this.lblSFX = new System.Windows.Forms.Label();
            this.lblBGM = new System.Windows.Forms.Label();
            this.gboxWindowSettings = new System.Windows.Forms.GroupBox();
            this.rbtnFullscreen = new System.Windows.Forms.RadioButton();
            this.rbtnWindowMode = new System.Windows.Forms.RadioButton();
            this.gboxGraphicSettings = new System.Windows.Forms.GroupBox();
            this.cboxDropItem = new System.Windows.Forms.ComboBox();
            this.lblDropItem = new System.Windows.Forms.Label();
            this.cboxPrivateShop = new System.Windows.Forms.ComboBox();
            this.lblPrivateShop = new System.Windows.Forms.Label();
            this.cboxEffect = new System.Windows.Forms.ComboBox();
            this.lblEffects = new System.Windows.Forms.Label();
            this.cboxShadowQuality = new System.Windows.Forms.ComboBox();
            this.lblShadowQuality = new System.Windows.Forms.Label();
            this.cboxTargetShadow = new System.Windows.Forms.ComboBox();
            this.lblShadowTarget = new System.Windows.Forms.Label();
            this.gboxLanguage = new System.Windows.Forms.GroupBox();
            this.cboxLocale = new System.Windows.Forms.ComboBox();
            this.gboxNightMode = new System.Windows.Forms.GroupBox();
            this.rbtnNightOff = new System.Windows.Forms.RadioButton();
            this.rbtnNightOn = new System.Windows.Forms.RadioButton();
            this.gboxNPCName = new System.Windows.Forms.GroupBox();
            this.rbtnNPCNameOff = new System.Windows.Forms.RadioButton();
            this.rbtnNPCNameOn = new System.Windows.Forms.RadioButton();
            this.gboxFogMode = new System.Windows.Forms.GroupBox();
            this.rbtnFogOff = new System.Windows.Forms.RadioButton();
            this.rbtnFogOn = new System.Windows.Forms.RadioButton();
            this.gboxSnowMode = new System.Windows.Forms.GroupBox();
            this.rbtnSnowOff = new System.Windows.Forms.RadioButton();
            this.rbtnSnowOn = new System.Windows.Forms.RadioButton();
            this.gboxSnowTextureMode = new System.Windows.Forms.GroupBox();
            this.rbtnSnowTextureOff = new System.Windows.Forms.RadioButton();
            this.rbtnSnowTextureOn = new System.Windows.Forms.RadioButton();
            this.gboxPetMode = new System.Windows.Forms.GroupBox();
            this.rbtnPetOff = new System.Windows.Forms.RadioButton();
            this.rbtnPetOn = new System.Windows.Forms.RadioButton();
            this.toolTip1 = new System.Windows.Forms.ToolTip(this.components);
            this.pboxDarkMode = new System.Windows.Forms.PictureBox();
            this.gboxDisplaySettings.SuspendLayout();
            this.gboxSoundSettings.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.trbarSFX)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.trbarBGM)).BeginInit();
            this.gboxWindowSettings.SuspendLayout();
            this.gboxGraphicSettings.SuspendLayout();
            this.gboxLanguage.SuspendLayout();
            this.gboxNightMode.SuspendLayout();
            this.gboxNPCName.SuspendLayout();
            this.gboxFogMode.SuspendLayout();
            this.gboxSnowMode.SuspendLayout();
            this.gboxSnowTextureMode.SuspendLayout();
            this.gboxPetMode.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pboxDarkMode)).BeginInit();
            this.SuspendLayout();
            // 
            // btnOK
            // 
            this.btnOK.Font = new System.Drawing.Font("Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnOK.Location = new System.Drawing.Point(128, 461);
            this.btnOK.Name = "btnOK";
            this.btnOK.Size = new System.Drawing.Size(90, 25);
            this.btnOK.TabIndex = 2;
            this.btnOK.UseVisualStyleBackColor = true;
            this.btnOK.Click += new System.EventHandler(this.btnOK_Click);
            // 
            // btnCANCEL
            // 
            this.btnCANCEL.Font = new System.Drawing.Font("Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(162)));
            this.btnCANCEL.Location = new System.Drawing.Point(224, 461);
            this.btnCANCEL.Name = "btnCANCEL";
            this.btnCANCEL.Size = new System.Drawing.Size(90, 25);
            this.btnCANCEL.TabIndex = 3;
            this.btnCANCEL.UseVisualStyleBackColor = true;
            this.btnCANCEL.Click += new System.EventHandler(this.btnCANCEL_Click);
            // 
            // gboxDisplaySettings
            // 
            this.gboxDisplaySettings.Controls.Add(this.cboxGamma);
            this.gboxDisplaySettings.Controls.Add(this.cboxFrequency);
            this.gboxDisplaySettings.Controls.Add(this.cboxResolution);
            this.gboxDisplaySettings.Controls.Add(this.lblGamma);
            this.gboxDisplaySettings.Controls.Add(this.lblFrequency);
            this.gboxDisplaySettings.Controls.Add(this.lblResolution);
            this.gboxDisplaySettings.Location = new System.Drawing.Point(12, 7);
            this.gboxDisplaySettings.Name = "gboxDisplaySettings";
            this.gboxDisplaySettings.Size = new System.Drawing.Size(205, 93);
            this.gboxDisplaySettings.TabIndex = 4;
            this.gboxDisplaySettings.TabStop = false;
            // 
            // cboxGamma
            // 
            this.cboxGamma.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxGamma.FormattingEnabled = true;
            this.cboxGamma.Items.AddRange(new object[] {
            "1",
            "2",
            "3",
            "4",
            "5"});
            this.cboxGamma.Location = new System.Drawing.Point(78, 66);
            this.cboxGamma.Name = "cboxGamma";
            this.cboxGamma.Size = new System.Drawing.Size(121, 22);
            this.cboxGamma.TabIndex = 13;
            // 
            // cboxFrequency
            // 
            this.cboxFrequency.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxFrequency.FormattingEnabled = true;
            this.cboxFrequency.Location = new System.Drawing.Point(78, 40);
            this.cboxFrequency.Name = "cboxFrequency";
            this.cboxFrequency.Size = new System.Drawing.Size(121, 22);
            this.cboxFrequency.TabIndex = 12;
            // 
            // cboxResolution
            // 
            this.cboxResolution.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxResolution.FormattingEnabled = true;
            this.cboxResolution.Location = new System.Drawing.Point(78, 15);
            this.cboxResolution.Name = "cboxResolution";
            this.cboxResolution.Size = new System.Drawing.Size(121, 22);
            this.cboxResolution.TabIndex = 11;
            this.cboxResolution.SelectedIndexChanged += new System.EventHandler(this.cboxResolution_SelectedIndexChanged);
            // 
            // lblGamma
            // 
            this.lblGamma.AutoSize = true;
            this.lblGamma.Location = new System.Drawing.Point(6, 66);
            this.lblGamma.Name = "lblGamma";
            this.lblGamma.Size = new System.Drawing.Size(0, 14);
            this.lblGamma.TabIndex = 10;
            // 
            // lblFrequency
            // 
            this.lblFrequency.AutoSize = true;
            this.lblFrequency.Location = new System.Drawing.Point(6, 40);
            this.lblFrequency.Name = "lblFrequency";
            this.lblFrequency.Size = new System.Drawing.Size(0, 14);
            this.lblFrequency.TabIndex = 9;
            // 
            // lblResolution
            // 
            this.lblResolution.AutoSize = true;
            this.lblResolution.Location = new System.Drawing.Point(6, 18);
            this.lblResolution.Name = "lblResolution";
            this.lblResolution.Size = new System.Drawing.Size(0, 14);
            this.lblResolution.TabIndex = 8;
            // 
            // gboxSoundSettings
            // 
            this.gboxSoundSettings.Controls.Add(this.trbarSFX);
            this.gboxSoundSettings.Controls.Add(this.trbarBGM);
            this.gboxSoundSettings.Controls.Add(this.tboxSFX);
            this.gboxSoundSettings.Controls.Add(this.tboxBGM);
            this.gboxSoundSettings.Controls.Add(this.groupBox6);
            this.gboxSoundSettings.Controls.Add(this.lblSFX);
            this.gboxSoundSettings.Controls.Add(this.lblBGM);
            this.gboxSoundSettings.Location = new System.Drawing.Point(12, 105);
            this.gboxSoundSettings.Name = "gboxSoundSettings";
            this.gboxSoundSettings.Size = new System.Drawing.Size(205, 59);
            this.gboxSoundSettings.TabIndex = 0;
            this.gboxSoundSettings.TabStop = false;
            this.gboxSoundSettings.Tag = "";
            // 
            // trbarSFX
            // 
            this.trbarSFX.AutoSize = false;
            this.trbarSFX.LargeChange = 25;
            this.trbarSFX.Location = new System.Drawing.Point(78, 35);
            this.trbarSFX.Maximum = 50;
            this.trbarSFX.Name = "trbarSFX";
            this.trbarSFX.Size = new System.Drawing.Size(121, 17);
            this.trbarSFX.SmallChange = 10;
            this.trbarSFX.TabIndex = 19;
            this.trbarSFX.TickFrequency = 0;
            this.trbarSFX.TickStyle = System.Windows.Forms.TickStyle.None;
            this.trbarSFX.Value = 50;
            this.trbarSFX.Scroll += new System.EventHandler(this.trbarSFX_Scroll);
            // 
            // trbarBGM
            // 
            this.trbarBGM.AutoSize = false;
            this.trbarBGM.LargeChange = 500;
            this.trbarBGM.Location = new System.Drawing.Point(78, 13);
            this.trbarBGM.Maximum = 1000;
            this.trbarBGM.Name = "trbarBGM";
            this.trbarBGM.Size = new System.Drawing.Size(121, 17);
            this.trbarBGM.SmallChange = 100;
            this.trbarBGM.TabIndex = 18;
            this.trbarBGM.TickFrequency = 0;
            this.trbarBGM.TickStyle = System.Windows.Forms.TickStyle.None;
            this.trbarBGM.Value = 1000;
            this.trbarBGM.Scroll += new System.EventHandler(this.trbarBGM_Scroll);
            // 
            // tboxSFX
            // 
            this.tboxSFX.Enabled = false;
            this.tboxSFX.Location = new System.Drawing.Point(51, 33);
            this.tboxSFX.Name = "tboxSFX";
            this.tboxSFX.Size = new System.Drawing.Size(25, 22);
            this.tboxSFX.TabIndex = 17;
            this.tboxSFX.Text = "5";
            this.tboxSFX.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            // 
            // tboxBGM
            // 
            this.tboxBGM.Enabled = false;
            this.tboxBGM.Location = new System.Drawing.Point(51, 11);
            this.tboxBGM.Name = "tboxBGM";
            this.tboxBGM.Size = new System.Drawing.Size(25, 22);
            this.tboxBGM.TabIndex = 13;
            this.tboxBGM.Text = "1.0";
            this.tboxBGM.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            // 
            // groupBox6
            // 
            this.groupBox6.Location = new System.Drawing.Point(204, 104);
            this.groupBox6.Name = "groupBox6";
            this.groupBox6.Size = new System.Drawing.Size(179, 36);
            this.groupBox6.TabIndex = 16;
            this.groupBox6.TabStop = false;
            this.groupBox6.Text = "YZK isimleri";
            // 
            // lblSFX
            // 
            this.lblSFX.AutoSize = true;
            this.lblSFX.Location = new System.Drawing.Point(5, 37);
            this.lblSFX.Name = "lblSFX";
            this.lblSFX.Size = new System.Drawing.Size(0, 14);
            this.lblSFX.TabIndex = 9;
            // 
            // lblBGM
            // 
            this.lblBGM.AutoSize = true;
            this.lblBGM.Location = new System.Drawing.Point(5, 16);
            this.lblBGM.Name = "lblBGM";
            this.lblBGM.Size = new System.Drawing.Size(0, 14);
            this.lblBGM.TabIndex = 8;
            // 
            // gboxWindowSettings
            // 
            this.gboxWindowSettings.Controls.Add(this.rbtnFullscreen);
            this.gboxWindowSettings.Controls.Add(this.rbtnWindowMode);
            this.gboxWindowSettings.Location = new System.Drawing.Point(224, 7);
            this.gboxWindowSettings.Name = "gboxWindowSettings";
            this.gboxWindowSettings.Size = new System.Drawing.Size(216, 36);
            this.gboxWindowSettings.TabIndex = 14;
            this.gboxWindowSettings.TabStop = false;
            // 
            // rbtnFullscreen
            // 
            this.rbtnFullscreen.AutoSize = true;
            this.rbtnFullscreen.Checked = true;
            this.rbtnFullscreen.Location = new System.Drawing.Point(112, 14);
            this.rbtnFullscreen.Name = "rbtnFullscreen";
            this.rbtnFullscreen.Size = new System.Drawing.Size(14, 13);
            this.rbtnFullscreen.TabIndex = 1;
            this.rbtnFullscreen.TabStop = true;
            this.rbtnFullscreen.UseVisualStyleBackColor = true;
            // 
            // rbtnWindowMode
            // 
            this.rbtnWindowMode.AutoSize = true;
            this.rbtnWindowMode.Location = new System.Drawing.Point(6, 14);
            this.rbtnWindowMode.Name = "rbtnWindowMode";
            this.rbtnWindowMode.Size = new System.Drawing.Size(14, 13);
            this.rbtnWindowMode.TabIndex = 0;
            this.rbtnWindowMode.UseVisualStyleBackColor = true;
            this.rbtnWindowMode.Click += new System.EventHandler(this.ShowSlowWarning);
            // 
            // gboxGraphicSettings
            // 
            this.gboxGraphicSettings.Controls.Add(this.cboxDropItem);
            this.gboxGraphicSettings.Controls.Add(this.lblDropItem);
            this.gboxGraphicSettings.Controls.Add(this.cboxPrivateShop);
            this.gboxGraphicSettings.Controls.Add(this.lblPrivateShop);
            this.gboxGraphicSettings.Controls.Add(this.cboxEffect);
            this.gboxGraphicSettings.Controls.Add(this.lblEffects);
            this.gboxGraphicSettings.Controls.Add(this.cboxShadowQuality);
            this.gboxGraphicSettings.Controls.Add(this.lblShadowQuality);
            this.gboxGraphicSettings.Controls.Add(this.cboxTargetShadow);
            this.gboxGraphicSettings.Controls.Add(this.lblShadowTarget);
            this.gboxGraphicSettings.Location = new System.Drawing.Point(224, 105);
            this.gboxGraphicSettings.Name = "gboxGraphicSettings";
            this.gboxGraphicSettings.Size = new System.Drawing.Size(216, 262);
            this.gboxGraphicSettings.TabIndex = 16;
            this.gboxGraphicSettings.TabStop = false;
            // 
            // cboxDropItem
            // 
            this.cboxDropItem.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxDropItem.FormattingEnabled = true;
            this.cboxDropItem.Items.AddRange(new object[] {
            "",
            "",
            "",
            "",
            ""});
            this.cboxDropItem.Location = new System.Drawing.Point(6, 225);
            this.cboxDropItem.Name = "cboxDropItem";
            this.cboxDropItem.Size = new System.Drawing.Size(204, 22);
            this.cboxDropItem.TabIndex = 27;
            // 
            // lblDropItem
            // 
            this.lblDropItem.AutoSize = true;
            this.lblDropItem.Location = new System.Drawing.Point(6, 207);
            this.lblDropItem.Name = "lblDropItem";
            this.lblDropItem.Size = new System.Drawing.Size(0, 14);
            this.lblDropItem.TabIndex = 26;
            // 
            // cboxPrivateShop
            // 
            this.cboxPrivateShop.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxPrivateShop.FormattingEnabled = true;
            this.cboxPrivateShop.Items.AddRange(new object[] {
            "",
            "",
            "",
            "",
            ""});
            this.cboxPrivateShop.Location = new System.Drawing.Point(6, 176);
            this.cboxPrivateShop.Name = "cboxPrivateShop";
            this.cboxPrivateShop.Size = new System.Drawing.Size(204, 22);
            this.cboxPrivateShop.TabIndex = 25;
            // 
            // lblPrivateShop
            // 
            this.lblPrivateShop.AutoSize = true;
            this.lblPrivateShop.Location = new System.Drawing.Point(6, 160);
            this.lblPrivateShop.Name = "lblPrivateShop";
            this.lblPrivateShop.Size = new System.Drawing.Size(0, 14);
            this.lblPrivateShop.TabIndex = 24;
            // 
            // cboxEffect
            // 
            this.cboxEffect.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxEffect.FormattingEnabled = true;
            this.cboxEffect.Items.AddRange(new object[] {
            "",
            "",
            "",
            "",
            ""});
            this.cboxEffect.Location = new System.Drawing.Point(6, 133);
            this.cboxEffect.Name = "cboxEffect";
            this.cboxEffect.Size = new System.Drawing.Size(204, 22);
            this.cboxEffect.TabIndex = 23;
            // 
            // lblEffects
            // 
            this.lblEffects.AutoSize = true;
            this.lblEffects.Location = new System.Drawing.Point(6, 116);
            this.lblEffects.Name = "lblEffects";
            this.lblEffects.Size = new System.Drawing.Size(0, 14);
            this.lblEffects.TabIndex = 22;
            // 
            // cboxShadowQuality
            // 
            this.cboxShadowQuality.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxShadowQuality.FormattingEnabled = true;
            this.cboxShadowQuality.Items.AddRange(new object[] {
            "",
            "",
            ""});
            this.cboxShadowQuality.Location = new System.Drawing.Point(6, 82);
            this.cboxShadowQuality.Name = "cboxShadowQuality";
            this.cboxShadowQuality.Size = new System.Drawing.Size(204, 22);
            this.cboxShadowQuality.TabIndex = 21;
            // 
            // lblShadowQuality
            // 
            this.lblShadowQuality.AutoSize = true;
            this.lblShadowQuality.Location = new System.Drawing.Point(6, 65);
            this.lblShadowQuality.Name = "lblShadowQuality";
            this.lblShadowQuality.Size = new System.Drawing.Size(0, 14);
            this.lblShadowQuality.TabIndex = 20;
            // 
            // cboxTargetShadow
            // 
            this.cboxTargetShadow.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxTargetShadow.FormattingEnabled = true;
            this.cboxTargetShadow.Items.AddRange(new object[] {
            "",
            "",
            "",
            ""});
            this.cboxTargetShadow.Location = new System.Drawing.Point(6, 37);
            this.cboxTargetShadow.Name = "cboxTargetShadow";
            this.cboxTargetShadow.Size = new System.Drawing.Size(204, 22);
            this.cboxTargetShadow.TabIndex = 19;
            // 
            // lblShadowTarget
            // 
            this.lblShadowTarget.AutoSize = true;
            this.lblShadowTarget.Location = new System.Drawing.Point(6, 21);
            this.lblShadowTarget.Name = "lblShadowTarget";
            this.lblShadowTarget.Size = new System.Drawing.Size(0, 14);
            this.lblShadowTarget.TabIndex = 18;
            // 
            // gboxLanguage
            // 
            this.gboxLanguage.Controls.Add(this.cboxLocale);
            this.gboxLanguage.Location = new System.Drawing.Point(12, 404);
            this.gboxLanguage.Name = "gboxLanguage";
            this.gboxLanguage.Size = new System.Drawing.Size(428, 51);
            this.gboxLanguage.TabIndex = 17;
            this.gboxLanguage.TabStop = false;
            // 
            // cboxLocale
            // 
            this.cboxLocale.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cboxLocale.FormattingEnabled = true;
            this.cboxLocale.Location = new System.Drawing.Point(17, 20);
            this.cboxLocale.Name = "cboxLocale";
            this.cboxLocale.Size = new System.Drawing.Size(386, 22);
            this.cboxLocale.TabIndex = 14;
            this.cboxLocale.SelectedIndexChanged += new System.EventHandler(this.cboxLocale_SelectedIndexChanged);
            // 
            // gboxNightMode
            // 
            this.gboxNightMode.Controls.Add(this.rbtnNightOff);
            this.gboxNightMode.Controls.Add(this.rbtnNightOn);
            this.gboxNightMode.Location = new System.Drawing.Point(12, 166);
            this.gboxNightMode.Name = "gboxNightMode";
            this.gboxNightMode.Size = new System.Drawing.Size(205, 37);
            this.gboxNightMode.TabIndex = 15;
            this.gboxNightMode.TabStop = false;
            // 
            // rbtnNightOff
            // 
            this.rbtnNightOff.AutoSize = true;
            this.rbtnNightOff.Checked = true;
            this.rbtnNightOff.Location = new System.Drawing.Point(116, 15);
            this.rbtnNightOff.Name = "rbtnNightOff";
            this.rbtnNightOff.Size = new System.Drawing.Size(14, 13);
            this.rbtnNightOff.TabIndex = 1;
            this.rbtnNightOff.TabStop = true;
            this.rbtnNightOff.UseVisualStyleBackColor = true;
            // 
            // rbtnNightOn
            // 
            this.rbtnNightOn.AutoSize = true;
            this.rbtnNightOn.Location = new System.Drawing.Point(19, 15);
            this.rbtnNightOn.Name = "rbtnNightOn";
            this.rbtnNightOn.Size = new System.Drawing.Size(14, 13);
            this.rbtnNightOn.TabIndex = 0;
            this.rbtnNightOn.UseVisualStyleBackColor = true;
            // 
            // gboxNPCName
            // 
            this.gboxNPCName.Controls.Add(this.rbtnNPCNameOff);
            this.gboxNPCName.Controls.Add(this.rbtnNPCNameOn);
            this.gboxNPCName.Location = new System.Drawing.Point(224, 371);
            this.gboxNPCName.Name = "gboxNPCName";
            this.gboxNPCName.Size = new System.Drawing.Size(216, 36);
            this.gboxNPCName.TabIndex = 18;
            this.gboxNPCName.TabStop = false;
            // 
            // rbtnNPCNameOff
            // 
            this.rbtnNPCNameOff.AutoSize = true;
            this.rbtnNPCNameOff.Location = new System.Drawing.Point(116, 13);
            this.rbtnNPCNameOff.Name = "rbtnNPCNameOff";
            this.rbtnNPCNameOff.Size = new System.Drawing.Size(14, 13);
            this.rbtnNPCNameOff.TabIndex = 1;
            this.rbtnNPCNameOff.UseVisualStyleBackColor = true;
            // 
            // rbtnNPCNameOn
            // 
            this.rbtnNPCNameOn.AutoSize = true;
            this.rbtnNPCNameOn.Checked = true;
            this.rbtnNPCNameOn.Location = new System.Drawing.Point(19, 13);
            this.rbtnNPCNameOn.Name = "rbtnNPCNameOn";
            this.rbtnNPCNameOn.Size = new System.Drawing.Size(14, 13);
            this.rbtnNPCNameOn.TabIndex = 0;
            this.rbtnNPCNameOn.TabStop = true;
            this.rbtnNPCNameOn.UseVisualStyleBackColor = true;
            // 
            // gboxFogMode
            // 
            this.gboxFogMode.Controls.Add(this.rbtnFogOff);
            this.gboxFogMode.Controls.Add(this.rbtnFogOn);
            this.gboxFogMode.Location = new System.Drawing.Point(12, 207);
            this.gboxFogMode.Name = "gboxFogMode";
            this.gboxFogMode.Size = new System.Drawing.Size(205, 37);
            this.gboxFogMode.TabIndex = 16;
            this.gboxFogMode.TabStop = false;
            // 
            // rbtnFogOff
            // 
            this.rbtnFogOff.AutoSize = true;
            this.rbtnFogOff.Location = new System.Drawing.Point(116, 15);
            this.rbtnFogOff.Name = "rbtnFogOff";
            this.rbtnFogOff.Size = new System.Drawing.Size(14, 13);
            this.rbtnFogOff.TabIndex = 1;
            this.rbtnFogOff.UseVisualStyleBackColor = true;
            // 
            // rbtnFogOn
            // 
            this.rbtnFogOn.AutoSize = true;
            this.rbtnFogOn.Checked = true;
            this.rbtnFogOn.Location = new System.Drawing.Point(19, 15);
            this.rbtnFogOn.Name = "rbtnFogOn";
            this.rbtnFogOn.Size = new System.Drawing.Size(14, 13);
            this.rbtnFogOn.TabIndex = 0;
            this.rbtnFogOn.TabStop = true;
            this.rbtnFogOn.UseVisualStyleBackColor = true;
            // 
            // gboxSnowMode
            // 
            this.gboxSnowMode.Controls.Add(this.rbtnSnowOff);
            this.gboxSnowMode.Controls.Add(this.rbtnSnowOn);
            this.gboxSnowMode.Location = new System.Drawing.Point(12, 247);
            this.gboxSnowMode.Name = "gboxSnowMode";
            this.gboxSnowMode.Size = new System.Drawing.Size(205, 37);
            this.gboxSnowMode.TabIndex = 16;
            this.gboxSnowMode.TabStop = false;
            // 
            // rbtnSnowOff
            // 
            this.rbtnSnowOff.AutoSize = true;
            this.rbtnSnowOff.Checked = true;
            this.rbtnSnowOff.Location = new System.Drawing.Point(116, 15);
            this.rbtnSnowOff.Name = "rbtnSnowOff";
            this.rbtnSnowOff.Size = new System.Drawing.Size(14, 13);
            this.rbtnSnowOff.TabIndex = 1;
            this.rbtnSnowOff.TabStop = true;
            this.rbtnSnowOff.UseVisualStyleBackColor = true;
            // 
            // rbtnSnowOn
            // 
            this.rbtnSnowOn.AutoSize = true;
            this.rbtnSnowOn.Location = new System.Drawing.Point(19, 15);
            this.rbtnSnowOn.Name = "rbtnSnowOn";
            this.rbtnSnowOn.Size = new System.Drawing.Size(14, 13);
            this.rbtnSnowOn.TabIndex = 0;
            this.rbtnSnowOn.UseVisualStyleBackColor = true;
            // 
            // gboxSnowTextureMode
            // 
            this.gboxSnowTextureMode.Controls.Add(this.rbtnSnowTextureOff);
            this.gboxSnowTextureMode.Controls.Add(this.rbtnSnowTextureOn);
            this.gboxSnowTextureMode.Location = new System.Drawing.Point(12, 288);
            this.gboxSnowTextureMode.Name = "gboxSnowTextureMode";
            this.gboxSnowTextureMode.Size = new System.Drawing.Size(205, 37);
            this.gboxSnowTextureMode.TabIndex = 16;
            this.gboxSnowTextureMode.TabStop = false;
            // 
            // rbtnSnowTextureOff
            // 
            this.rbtnSnowTextureOff.AutoSize = true;
            this.rbtnSnowTextureOff.Checked = true;
            this.rbtnSnowTextureOff.Location = new System.Drawing.Point(116, 15);
            this.rbtnSnowTextureOff.Name = "rbtnSnowTextureOff";
            this.rbtnSnowTextureOff.Size = new System.Drawing.Size(14, 13);
            this.rbtnSnowTextureOff.TabIndex = 1;
            this.rbtnSnowTextureOff.TabStop = true;
            this.rbtnSnowTextureOff.UseVisualStyleBackColor = true;
            // 
            // rbtnSnowTextureOn
            // 
            this.rbtnSnowTextureOn.AutoSize = true;
            this.rbtnSnowTextureOn.Location = new System.Drawing.Point(19, 15);
            this.rbtnSnowTextureOn.Name = "rbtnSnowTextureOn";
            this.rbtnSnowTextureOn.Size = new System.Drawing.Size(14, 13);
            this.rbtnSnowTextureOn.TabIndex = 0;
            this.rbtnSnowTextureOn.UseVisualStyleBackColor = true;
            // 
            // gboxPetMode
            // 
            this.gboxPetMode.Controls.Add(this.rbtnPetOff);
            this.gboxPetMode.Controls.Add(this.rbtnPetOn);
            this.gboxPetMode.Location = new System.Drawing.Point(12, 329);
            this.gboxPetMode.Name = "gboxPetMode";
            this.gboxPetMode.Size = new System.Drawing.Size(205, 37);
            this.gboxPetMode.TabIndex = 16;
            this.gboxPetMode.TabStop = false;
            // 
            // rbtnPetOff
            // 
            this.rbtnPetOff.AutoSize = true;
            this.rbtnPetOff.Location = new System.Drawing.Point(116, 15);
            this.rbtnPetOff.Name = "rbtnPetOff";
            this.rbtnPetOff.Size = new System.Drawing.Size(14, 13);
            this.rbtnPetOff.TabIndex = 1;
            this.rbtnPetOff.UseVisualStyleBackColor = true;
            // 
            // rbtnPetOn
            // 
            this.rbtnPetOn.AutoSize = true;
            this.rbtnPetOn.Checked = true;
            this.rbtnPetOn.Location = new System.Drawing.Point(19, 15);
            this.rbtnPetOn.Name = "rbtnPetOn";
            this.rbtnPetOn.Size = new System.Drawing.Size(14, 13);
            this.rbtnPetOn.TabIndex = 0;
            this.rbtnPetOn.TabStop = true;
            this.rbtnPetOn.UseVisualStyleBackColor = true;
            // 
            // pboxDarkMode
            // 
            this.pboxDarkMode.Image = global::Metin2Config.Properties.Resources.night_off;
            this.pboxDarkMode.Location = new System.Drawing.Point(416, 464);
            this.pboxDarkMode.Name = "pboxDarkMode";
            this.pboxDarkMode.Size = new System.Drawing.Size(24, 22);
            this.pboxDarkMode.TabIndex = 8;
            this.pboxDarkMode.TabStop = false;
            this.toolTip1.SetToolTip(this.pboxDarkMode, "Dark Mode");
            this.pboxDarkMode.Visible = false;
            this.pboxDarkMode.Click += new System.EventHandler(this.pboxDarkMode_Click);
            // 
            // FormConfig
            // 
            this.AcceptButton = this.btnOK;
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 14F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(452, 494);
            this.Controls.Add(this.gboxPetMode);
            this.Controls.Add(this.gboxSnowTextureMode);
            this.Controls.Add(this.gboxSnowMode);
            this.Controls.Add(this.gboxFogMode);
            this.Controls.Add(this.gboxNPCName);
            this.Controls.Add(this.gboxNightMode);
            this.Controls.Add(this.gboxLanguage);
            this.Controls.Add(this.gboxGraphicSettings);
            this.Controls.Add(this.gboxWindowSettings);
            this.Controls.Add(this.pboxDarkMode);
            this.Controls.Add(this.gboxSoundSettings);
            this.Controls.Add(this.gboxDisplaySettings);
            this.Controls.Add(this.btnCANCEL);
            this.Controls.Add(this.btnOK);
            this.Font = new System.Drawing.Font("Tahoma", 9F);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "FormConfig";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = " ";
            this.Load += new System.EventHandler(this.FormConfig_Load);
            this.gboxDisplaySettings.ResumeLayout(false);
            this.gboxDisplaySettings.PerformLayout();
            this.gboxSoundSettings.ResumeLayout(false);
            this.gboxSoundSettings.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.trbarSFX)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.trbarBGM)).EndInit();
            this.gboxWindowSettings.ResumeLayout(false);
            this.gboxWindowSettings.PerformLayout();
            this.gboxGraphicSettings.ResumeLayout(false);
            this.gboxGraphicSettings.PerformLayout();
            this.gboxLanguage.ResumeLayout(false);
            this.gboxNightMode.ResumeLayout(false);
            this.gboxNightMode.PerformLayout();
            this.gboxNPCName.ResumeLayout(false);
            this.gboxNPCName.PerformLayout();
            this.gboxFogMode.ResumeLayout(false);
            this.gboxFogMode.PerformLayout();
            this.gboxSnowMode.ResumeLayout(false);
            this.gboxSnowMode.PerformLayout();
            this.gboxSnowTextureMode.ResumeLayout(false);
            this.gboxSnowTextureMode.PerformLayout();
            this.gboxPetMode.ResumeLayout(false);
            this.gboxPetMode.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pboxDarkMode)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion
        private Button btnOK;
        private Button btnCANCEL;
        private GroupBox gboxDisplaySettings;
        private GroupBox gboxSoundSettings;
        private Label lblFrequency;
        private Label lblResolution;
        private Label lblGamma;
        private ComboBox cboxGamma;
        private ComboBox cboxFrequency;
        private ComboBox cboxResolution;
        private Label lblSFX;
        private Label lblBGM;
        private TextBox tboxBGM;
        private PictureBox pboxDarkMode;
        private GroupBox gboxWindowSettings;
        private GroupBox gboxGraphicSettings;
        private GroupBox groupBox6;
        private GroupBox gboxLanguage;
        private ComboBox cboxLocale;
        private GroupBox gboxNightMode;
        private RadioButton rbtnNightOff;
        private RadioButton rbtnNightOn;
        private GroupBox gboxNPCName;
        private RadioButton rbtnNPCNameOff;
        private RadioButton rbtnNPCNameOn;
        private GroupBox gboxFogMode;
        private RadioButton rbtnFogOff;
        private RadioButton rbtnFogOn;
        private GroupBox gboxSnowMode;
        private RadioButton rbtnSnowOff;
        private RadioButton rbtnSnowOn;
        private GroupBox gboxSnowTextureMode;
        private RadioButton rbtnSnowTextureOff;
        private RadioButton rbtnSnowTextureOn;
        private GroupBox gboxPetMode;
        private RadioButton rbtnPetOff;
        private RadioButton rbtnPetOn;
        private RadioButton rbtnFullscreen;
        private RadioButton rbtnWindowMode;
        private TextBox tboxSFX;
        private ComboBox cboxDropItem;
        private Label lblDropItem;
        private ComboBox cboxPrivateShop;
        private Label lblPrivateShop;
        private ComboBox cboxEffect;
        private Label lblEffects;
        private ComboBox cboxShadowQuality;
        private Label lblShadowQuality;
        private ComboBox cboxTargetShadow;
        private Label lblShadowTarget;
        private ToolTip toolTip1;
        private TrackBar trbarBGM;
        private TrackBar trbarSFX;
    }
}