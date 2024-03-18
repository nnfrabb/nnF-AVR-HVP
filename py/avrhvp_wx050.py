from cProfile import label
from numpy import size
from wx import *
import os, sys, glob, serial, time, crcmod
import wx.lib.scrolledpanel
import os.path
import serial.tools.list_ports
import json

class MainWindow(wx.Frame):
    def __init__(self, parent, title):
        self.pBUADRATE = 256000
        self.Fuse_Conf = ''
        self.chipID = ''
        self.chipName = ''
        self.memSize = ''
        self.eepSize = ''
        self.pageSize = ''
        self.hexSize = ''
        self.HEXdata = ''
        self.srtADDR = ''

        self.dirname = ''
        self.filename = ''
        self.portSelect = ''

        self.MCU_List = ''
        with open('fuse_conf.json') as avr_conf:
            self.Fuse_Conf = json.load(avr_conf)
            self.MCU_List = dict(self.Fuse_Conf["MCU_Signature"])
        
        wx.Frame.__init__(self, parent, title=title, size=(600, 750),style = wx.DEFAULT_FRAME_STYLE & ~wx.MAXIMIZE_BOX ^ wx.RESIZE_BORDER)
        self.SetBackgroundColour((230, 225, 230))

        font1 = wx.Font(12, wx.MODERN, wx.NORMAL, wx.NORMAL, False, u'Courier New')
        font2 = wx.Font(12, wx.MODERN, wx.NORMAL, wx.FONTWEIGHT_BOLD, False, u'Courier New')

        self.txtCtrl = wx.TextCtrl(self, style=wx.TE_MULTILINE|wx.TE_READONLY|wx.HSCROLL|wx.TE_RICH,size=(600, 650))
        self.txtCtrl.SetFont(font1)
        self.txtCtrl.SetEditable(False)
        self.txtCtrl.SetBackgroundColour((250, 250, 250))
        self.txtCtrl.SetForegroundColour('blue')

        self.stbar = self.CreateStatusBar(3)
        self.stbar.SetFont(font1)
        self.stbar.SetStatusWidths([-1, 120, 100])
        self.stbar.SetStatusText('CRC:0xFFFF', 2)

        self.txtBox = wx.BoxSizer(wx.VERTICAL)
        self.txtBox.Add(self.txtCtrl, 1)
        self.txtBox.Fit(self)

        self.toolbar = self.CreateToolBar()
        self.toolbar.SetToolBitmapSize((32, 32))  # sets icon size

        new_ico = wx.Bitmap("png/021-contract.png",)
        newTool = self.toolbar.AddTool(1, "Open...", new_ico, "New Project.")
        self.Bind(wx.EVT_MENU, self.OnNew, newTool)

        open_ico = wx.Bitmap("png/022-open-book.png")
        openTool = self.toolbar.AddTool(2, "Open...", open_ico, "Open Hex File.")
        self.Bind(wx.EVT_MENU, self.OnOpen, openTool)

        reload_ico = wx.Bitmap("png/046-arrows.png")
        reloadTool = self.toolbar.AddTool(3, "Reload", reload_ico, "Reload the Current HEX File.")
        self.Bind(wx.EVT_MENU, self.OnReload, reloadTool)

        save_ico = wx.Bitmap("png/036-folder-2.png")
        saveTool = self.toolbar.AddTool(4, "Reload", save_ico, "Save the Current HEX data.")
        self.Bind(wx.EVT_MENU, self.OnSave, saveTool)

        self.toolbar.AddSeparator()

        uflash_ico = wx.Bitmap("png/008-arrows-5.png")
        upLoadFlashTool = self.toolbar.AddTool(6, "Test", uflash_ico, "Upload Op-Code to Device.")
        self.Bind(wx.EVT_MENU, self.OnUpload, upLoadFlashTool)

        rflash_ico = wx.Bitmap("png/017-arrows-4.png")
        readFlashTool = self.toolbar.AddTool(7, "Test", rflash_ico, "Read Program(Flash) Memory From Device")
        self.Bind(wx.EVT_MENU, self.OnDownload, readFlashTool)

        fusebit_ico = wx.Bitmap("png/024-cogwheel-1.png")
        fusebitTool = self.toolbar.AddTool(8, "Fuse bits", fusebit_ico, "Get Device Fuse bits.")
        self.Bind(wx.EVT_MENU, self.OnConfig, fusebitTool)

        eflash_ico = wx.Bitmap("png/013-can.png")
        ereseFlashTool = self.toolbar.AddTool(9, "Test", eflash_ico, "Erase Flash (Set All location to 0xFF).")
        self.Bind(wx.EVT_MENU, self.OnErase, ereseFlashTool)

        self.toolbar.AddSeparator()

        link_ico = wx.Bitmap("png/002-chain-1.png")
        syncTool = self.toolbar.AddTool(10, "Link", link_ico, "Auto Detect.")
        self.Bind(wx.EVT_MENU, self.OnSync, syncTool)

        self.toolbar.AddStretchableSpace()
        self.toolbar.AddSeparator()

        mcu_ico = wx.Bitmap("png/grayled32.png")
        mcuTool = self.toolbar.AddTool(11, "Exit", mcu_ico, "Board AVR-HVP not Connect.")
        self.Bind(wx.EVT_MENU, self.OnSync, mcuTool)
        # This basically shows the toolbar
        self.toolbar.SetBackgroundColour((200, 200, 100, 200))
        self.toolbar.Realize()

        for x in range(1, 10):
            self.toolbar.EnableTool(x, False)

        self.btmbtn = wx.BoxSizer(wx.HORIZONTAL)
        self.button = []
        lblBtn = ["Reset", "RST2I/O", "Max Clk", "Lock bit", "MCU info"]
        onbutton = [self.OnReset, self.OnRST2IO, self.OnMxClk, self.OnLckbit, self.onTest]
        n = 0
        for i in lblBtn:
            self.button.append(wx.Button(self, -1, i))
            self.button[n].SetFont(font1)
            self.button[n].SetForegroundColour(('blue'))
            self.button[n].Bind(wx.EVT_BUTTON, onbutton[n])
            self.btmbtn.Add(self.button[n], 1, wx.ALL | wx.EXPAND, 1)
            self.button[n].Enable(False)
            n = n+1

        self.boxportlist = wx.BoxSizer(wx.HORIZONTAL)
        self.lblport = wx.StaticText(self, wx.ID_ANY, " Port: ")
        self.lblport.SetFont(font1)
        self.lblport.SetBackgroundColour((230, 225, 230))

        sport = self.serial_ports()
        self.portList = wx.ComboBox(self, 100, 'PORT', (50, 50), (70, -1), choices=sport, style=wx.CB_DROPDOWN)
        self.Bind(wx.EVT_COMBOBOX, self.OnPortSelection)

        self.lbldef = wx.StaticText(self, wx.ID_ANY, " Device not connect.")
        self.lbldef.SetFont(font1)
        self.lbldef.SetBackgroundColour((230, 225, 230))

        self.boxportlist.Add(self.lblport, 0, wx.TOP | wx.EXPAND, 3)
        self.boxportlist.Add(self.portList, 0, wx.ALL | wx.EXPAND, 1)
        self.boxportlist.Add(self.lbldef, 0, wx.TOP | wx.EXPAND, 3)
        #"""
        self.fuseBoxm = wx.BoxSizer(wx.HORIZONTAL,)
        box1_title = wx.StaticBox( self,-1, "Fuse bits Configuration.",  style=wx.BORDER_SUNKEN)
        box1_title.SetFont(font2)
        #box1 = wx.StaticBox(self, size=(-1,-1), style=wx.SUNKEN_BORDER)        
        self.fuseBoxs = wx.StaticBoxSizer(box1_title, orient=wx.HORIZONTAL)

        self.fuseBoxl = wx.BoxSizer(wx.HORIZONTAL)
        self.lbllFuse = wx.StaticText(self, wx.ID_ANY, "Low:0x")
        self.lbllFuse.SetFont(font1)
        self.txtlFuse = wx.TextCtrl(self, style=wx.TE_LEFT|wx.TOP,size=(35, 20))
        self.txtlFuse.SetFont(font2)
        self.txtlFuse.SetForegroundColour(('red'))
        self.txtlFuse.AppendText("00")
        self.fuseBoxl.Add(self.lbllFuse, 0, wx.ALL, 0)
        self.fuseBoxl.Add(self.txtlFuse, 0, wx.ALL, 0)

        self.fuseBoxh = wx.BoxSizer(wx.HORIZONTAL)
        self.lblhFuse = wx.StaticText(self, wx.ID_ANY, "High:0x")
        self.lblhFuse.SetFont(font1)
        self.txthFuse = wx.TextCtrl(self, style=wx.TE_LEFT,size=(35, 20))
        self.txthFuse.SetFont(font2)
        self.txthFuse.SetForegroundColour(('red'))
        self.txthFuse.AppendText("00")
        self.fuseBoxh.Add(self.lblhFuse, 0, wx.ALL, 0)
        self.fuseBoxh.Add(self.txthFuse, 0, wx.ALL, 0)

        self.fuseBoxe = wx.BoxSizer(wx.HORIZONTAL)
        self.lbleFuse = wx.StaticText(self, wx.ID_ANY, "Ext.:0x")
        self.lbleFuse.SetFont(font1)
        self.txteFuse = wx.TextCtrl(self, style=wx.TE_LEFT|wx.TOP,size=(35, 20))
        self.txteFuse.SetFont(font2)
        self.txteFuse.SetForegroundColour(('red'))
        self.txteFuse.AppendText("00")
        self.fuseBoxe.Add(self.lbleFuse, 0, wx.ALL, 0)
        self.fuseBoxe.Add(self.txteFuse, 0, wx.ALL, 0)

        self.bntWrFuse = wx.Button(self, label="Write",size=(65, 45))
        self.bntWrFuse.SetFont(font2)
        self.bntWrFuse.SetForegroundColour(('red'))
        self.bntWrFuse.Bind(wx.EVT_BUTTON, self.onManWriteFuse)

        self.fuseBoxs.Add(self.fuseBoxl, 1, wx.ALL, 1)
        self.fuseBoxs.Add(self.fuseBoxh, 1, wx.ALL, 1)
        self.fuseBoxs.Add(self.fuseBoxe, 1, wx.ALL, 1)

        self.fuseBoxm.Add(self.fuseBoxs, 1, wx.ALL, 1)
        self.fuseBoxm.Add(self.bntWrFuse, border=5, flag=wx.LEFT | wx.TOP)
        #"""
        ###############################################################
        self.sizer = wx.BoxSizer(wx.VERTICAL)
        self.sizer.Add(self.btmbtn, 0,  wx.ALL|wx.EXPAND, 2)
        self.sizer.Add(self.boxportlist, -1, wx.ALL|wx.EXPAND, 2)
        self.sizer.Add(self.fuseBoxm, 1,  wx.ALL|wx.EXPAND, 2)
        self.sizer.Add(self.txtBox, 5, wx.EXPAND, 5)
        ###############################################################

        self.SetSizer(self.sizer)
        self.SetAutoLayout(True)
        self.sizer.Fit(self)
        self.Show()

        icon = wx.Icon()
        icon.CopyFromBitmap(wx.Bitmap("png/029-cogwheel.png", wx.BITMAP_TYPE_ANY))
        self.SetIcon(icon)

        self.toolbar.EnableTool(2, True)
        #self.OnLoad(self)
        #self.OnSync(self)

    def OnAbout(self, e):
        # Create a message dialog box
        dlg = wx.MessageDialog(self, " A sample editor \n in wxPython", "About Sample Editor", wx.OK)
        dlg.ShowModal() # Shows it
        dlg.Destroy() # finally destroy it when finished.
        self.txtCtrl.Clear()
        self.txtCtrl.LoadFile(os.path.join(self.dirname, self.filename))

    def OnExit(self, e):
        dlg = wx.MessageDialog(self, "===========\nExit Programm!\n===========", "Exit", wx.OK | wx.CANCEL)
        try:
            if dlg.ShowModal() == wx.ID_OK:
                self.Close(True)  # Close the frame.
            else:
                pass
        finally:
            dlg.Destroy()

    def OnOpen(self, e):
        dlg = wx.FileDialog(self, "Choose a HEX file", self.dirname, "", "*.HEX", wx.FD_OPEN)
        if dlg.ShowModal() == wx.ID_OK:
            self.filename = dlg.GetFilename()
            self.dirname = dlg.GetDirectory()
            crc16txt = self.CRC16_from_file()

            self.stbar.SetStatusWidths([-1, len(self.filename)*10+10, 100])

            self.stbar.SetStatusText("CRC:" + crc16txt, 2)
            self.stbar.SetStatusText(self.filename, 1)

            with open(os.path.join(self.dirname, self.filename), 'r') as f:
                self.txtCtrl.Clear()
                txtb = ''
                htxt = ''
                for line in f:
                    byteno = line[1:3]
                    if byteno != "00":
                        txt1 = ''
                        x = 7
                        for i in range(0, int(byteno, 16)):
                            x += 2
                            txt1 = txt1+" "+line[x:x+2]
                            htxt += line[x:x+2]
                        txt1 = line[1:3]+":"+line[3:7]+":"+txt1+'\n'
                        txtb += txt1
                txtb += "00:0000: 01 FF"
                self.txtCtrl.AppendText(txtb)
                self.HEXdata = htxt
                self.hexSize = int((len(self.HEXdata)/2) - 1)
            f.close()
            self.txtCtrl.SetInsertionPoint(0)
        dlg.Destroy()

    def serial_ports(self):
        if sys.platform.startswith('win'):
            ports = ['COM%s' % (i + 1) for i in range(256)]
        elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
            ports = glob.glob('/dev/tty[A-Za-z]*')
        elif sys.platform.startswith('darwin'):
            ports = glob.glob('/dev/tty.*')
        else:
            raise EnvironmentError('Unsupported platform')

        result = []
        for port in ports:
            try:
                s = serial.Serial(port)
                s.close()
                result.append(port)
            except (OSError, serial.SerialException):
                pass
        return result

    def OnLoad(self, e):
        txtHex = ""
        for i in range(0, 41):
            d1 = "0"
            d2 = "0"
            x = (i % 16)
            y = int(i / 16)
            if (x < 10):
                d1 = str(x)
            if (x == 10):
                d1 = "A"
            if (x == 11):
                d1 = "B"
            if (x == 12):
                d1 = "C"
            if (x == 13):
                d1 = "D"
            if (x == 14):
                d1 = "E"
            if (x == 15):
                d1 = "F"
            if (y > 0):
                d2 = str(y)

            txtData = ""
            for j in range(0, 16):
                txtData += " FF"
            txtHex += "10:0" + str(d2) + str(d1) + "0:" + txtData + "\n"
        #txtHex += "00:00000: 01 FF"
        self.txtCtrl.AppendText(txtHex)
        self.txtCtrl.SetInsertionPoint(0)

    def OnSave(self, e):
        with wx.FileDialog(self, "Save HEX file", wildcard="HEX files (*.hex)|*.hex",
                           style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT) as fileDialog:

            if fileDialog.ShowModal() == wx.ID_CANCEL:
                return  # the user changed their mind

            # save the current contents in the file
            pathname = fileDialog.GetPath()
            try:
                with open(pathname, 'w') as file:
                    for line in range(0, self.txtCtrl.GetNumberOfLines()):
                        tl = self.txtCtrl.GetLineText(line).replace(" ", "")
                        nn = tl[0:2]
                        aa = tl[4:8]
                        dd = tl[9:]
                        if nn != '00':
                            cph = nn + aa + "00" + dd
                            i = 0
                            if nn != '':
                                i = int(nn, 16) + 4
                                x, y = 0, 2
                                hc = 0
                                for nc in range(0, i):
                                    hc += int(cph[x:y], 16)
                                    x, y = x + 2, y + 2

                                csm = ((hc & 255) ^ 255) + 1
                                csmh = hex(csm)[2:].upper()
                                csmh2 = csmh
                                if(len(csmh)>2):
                                    csmh2 = csmh[1:]
                                csmhex = csmh2
                                if(len(csmh)==1):
                                    csmhex = "0" + csmh2
                                file.write(':' + cph + csmhex + '\n')
                    file.write(':00000001FF\n')
                    file.close()
            except IOError:
                wx.LogError("Cannot save current data in file '%s'." % pathname)

    def OnReset(self, e):
        dlg = wx.MessageDialog(self, "================\nFuse bits Factory Reset!\n================",
                               "Exit", wx.OK | wx.CANCEL)
        try:
            if dlg.ShowModal() == wx.ID_OK:
                nport = self.portList.GetValue()
                ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
                if ser.isOpen():
                    ser.write(b'+fctrst\n')
                    time.sleep(0.2)
                    spx = ser.readall()

                    lfuse = spx[14:16]
                    hfuse = spx[16:18]
                    efuse = spx[18:20]
                    self.txtlFuse.Clear()
                    self.txtlFuse.AppendText(lfuse)
                    self.txthFuse.Clear()
                    self.txthFuse.AppendText(hfuse)
                    self.txteFuse.Clear()
                    self.txteFuse.AppendText(efuse)
                else:
                    pass
                ser.close()
            else:
                pass
        finally:
            dlg.Destroy()

    def onManWriteFuse(self,e):
        dlg = wx.MessageDialog(self, "============\nFuse bits Change!.\n============",
                               "Exit", wx.OK | wx.CANCEL)
        try:
            if dlg.ShowModal() == wx.ID_OK:
                nport = self.portList.GetValue()
                ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
                if ser.isOpen():
                    lfuse = self.txtlFuse.GetValue()
                    hfuse = self.txthFuse.GetValue()
                    efuse = self.txteFuse.GetValue()
                    cmds = b'+wrfuse'+bytes(lfuse+hfuse+efuse, 'utf-8')+b'\n'
                    print(cmds)

                    ser.write(cmds)
                    time.sleep(0.2)
                    spx = ser.readall()

                    lfuse = spx[14:16]
                    hfuse = spx[16:18]
                    efuse = spx[18:20]
                    self.txtlFuse.Clear()
                    self.txtlFuse.AppendText(lfuse)
                    self.txthFuse.Clear()
                    self.txthFuse.AppendText(hfuse)
                    self.txteFuse.Clear()
                    self.txteFuse.AppendText(efuse)
                else:
                    pass
                ser.close()
            else:
                pass
        finally:
            dlg.Destroy()

    def OnRST2IO(self, e):
        dlg = wx.MessageDialog(self, "================\nChange RST to IO pin.\n================",
                               "Exit", wx.OK | wx.CANCEL)
        try:
            if dlg.ShowModal() == wx.ID_OK:
                nport = self.portList.GetValue()
                ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
                if ser.isOpen():
                    ser.write(b'+rstdis\n')
                    time.sleep(0.2)
                    spx = ser.readall()

                    lfuse = spx[14:16]
                    hfuse = spx[16:18]
                    efuse = spx[18:20]
                    self.txtlFuse.Clear()
                    self.txtlFuse.AppendText(lfuse)
                    self.txthFuse.Clear()
                    self.txthFuse.AppendText(hfuse)
                    self.txteFuse.Clear()
                    self.txteFuse.AppendText(efuse)
                else:
                    pass
                ser.close()
            else:
                pass
        finally:
            dlg.Destroy()

    def OnMxClk(self, e):
        dlg = wx.MessageDialog(self, "================\nSet internal OSC. to max speed.\n================",
                               "Exit", wx.OK | wx.CANCEL)
        try:
            if dlg.ShowModal() == wx.ID_OK:
                nport = self.portList.GetValue()
                ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
                if ser.isOpen():
                    ser.write(b'+maxclk\n')
                    time.sleep(0.2)
                    spx = ser.readall()

                    lfuse = spx[14:16]
                    hfuse = spx[16:18]
                    efuse = spx[18:20]
                    self.txtlFuse.Clear()
                    self.txtlFuse.AppendText(lfuse)
                    self.txthFuse.Clear()
                    self.txthFuse.AppendText(hfuse)
                    self.txteFuse.Clear()
                    self.txteFuse.AppendText(efuse)
                else:
                    pass

                ser.close()
            else:
                pass
        finally:
            dlg.Destroy()

    def OnLckbit(self, e):
        dlg = wx.MessageDialog(self, "================\nEnable Lock bit.\n================",
                               "Exit", wx.OK | wx.CANCEL)
        try:
            if dlg.ShowModal() == wx.ID_OK:
                nport = self.portList.GetValue()
                ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
                if ser.isOpen():
                    ser.write(b'+lckbit\n')
                    time.sleep(0.2)
                    spx = ser.readall()

                    lfuse = spx[14:16]
                    hfuse = spx[16:18]
                    efuse = spx[18:20]
                    self.txtlFuse.Clear()
                    self.txtlFuse.AppendText(lfuse)
                    self.txthFuse.Clear()
                    self.txthFuse.AppendText(hfuse)
                    self.txteFuse.Clear()
                    self.txteFuse.AppendText(efuse)
                else:
                    pass
                ser.close()
            else:
                pass
        finally:
            dlg.Destroy()

    def OnErase(self, e):
        dlg = wx.MessageDialog(self, "================\nErase Flash.\n================",
                               "Exit", wx.OK | wx.CANCEL)
        try:
            if dlg.ShowModal() == wx.ID_OK:
                nport = self.portList.GetValue()
                ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
                if ser.isOpen():
                    ser.write(b'+erchip\n')
                    time.sleep(0.2)
                    spx = ser.readall()

                    lfuse = spx[14:16]
                    hfuse = spx[16:18]
                    efuse = spx[18:20]
                    self.txtlFuse.Clear()
                    self.txtlFuse.AppendText(lfuse)
                    self.txthFuse.Clear()
                    self.txthFuse.AppendText(hfuse)
                    self.txteFuse.Clear()
                    self.txteFuse.AppendText(efuse)
                else:
                    pass
                ser.close()
            else:
                pass
        finally:
            dlg.Destroy()

    def mcuInf(self, e):
        print("On MCU Inf...")

    def OnReload(self, e):
        try:
            with open(os.path.join(self.dirname, self.filename), 'r') as f:
                crc16txt = self.CRC16_from_file()
                self.stbar.SetStatusText("CRC:" + crc16txt, 2)
                self.txtCtrl.Clear()
                txtb = ''
                dhex = ''
                for line in f:
                    byteno = line[1:3]
                    if byteno != "00":
                        txt1 = ''
                        x = 7
                        for i in range(0, int(byteno, 16)):
                            x += 2
                            txt1 = txt1 + " " + line[x:x + 2]
                            dhex += line[x:x+2]

                        txt1 = line[1:3] + ":0" + line[3:7] + ":" + txt1 + '\n'
                        txtb += txt1
                #txtb += "00:00000: 01 FF"
                self.txtCtrl.AppendText(txtb)
                self.txtCtrl.SetInsertionPoint(0)
                self.HEXdata = dhex
                self.hexSize = int((len(self.HEXdata) / 2) - 1)
                print(self.hexSize)

                crc16txt = self.CRC16_from_file()
                self.stbar.SetStatusWidths([-1, len(self.filename) * 10 + 10, 100])
                self.stbar.SetStatusText("CRC:" + crc16txt, 2)
                self.stbar.SetStatusText(self.filename, 1)

            f.close()
        except(AttributeError):
            self.OnOpen(self) 
        except(FileNotFoundError):
            self.OnOpen(self) 

    def OnConfig(self, e):
        port = self.portList.GetValue()
        ser = serial.Serial(port, self.pBUADRATE, timeout=1)
        ser.write(b'\n')
        time.sleep(0.2)
        x = ser.readall()
        #"""
        lfuse = x[14:16]
        hfuse = x[16:18]
        efuse = x[18:20]
        self.txtlFuse.Clear()
        self.txtlFuse.AppendText(lfuse)
        self.txthFuse.Clear()
        self.txthFuse.AppendText(hfuse)
        self.txteFuse.Clear()
        self.txteFuse.AppendText(efuse)
        #"""
        ser.close()

    def OnNew(self, e):
        self.stbar.SetStatusText('CRC:0xFFFF', 2)
        self.stbar.SetStatusText("", 1)
        self.txtCtrl.Clear()
        txtHex = ""
        for i in range(0, 41):
            d1 = "0"
            d2 = "0"
            x = (i % 16)
            y = int(i / 16)
            if (x < 10):
                d1 = str(x)
            if (x == 10):
                d1 = "A"
            if (x == 11):
                d1 = "B"
            if (x == 12):
                d1 = "C"
            if (x == 13):
                d1 = "D"
            if (x == 14):
                d1 = "E"
            if (x == 15):
                d1 = "F"
            if (y > 0):
                d2 = str(y)

            txtData = ""
            for j in range(0, 16):
                txtData += " FF"
            txtHex += "10:00" + str(d2) + str(d1) + "0:" + txtData + "\n"
        #txtHex += "00:00000: 01 FF"
        self.txtCtrl.AppendText(txtHex)
        self.txtCtrl.SetInsertionPoint(0)

    def OnPortSelection(self, e):
        nport = self.portList.GetValue()
        ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
        if ser.isOpen():
            ser.write(b'+getsig\n')
            x = ser.readline()
            print(x)
            if (len(x) > 6):
                self.chipID = x[0:6]
                if self.chipID != b'000000':
                    self.chipName = self.chipDataInfo[self.chipID]
                    self.memSize = self.memSizeInfo[self.chipID]
                    self.pageSize = self.pageSizeInfo[self.chipID]

                    self.lbldef.SetLabel(" AVR-HVP:Connected to >>> " + self.chipName)
                    self.toolbar.DeleteTool(11)
                    exit_ico = wx.Bitmap("png/greenled32.png")
                    self.toolbar.AddTool(11, "Connected", exit_ico, "Board AVR-HVP Connected.")
                    self.toolbar.Realize()

                    for x in range(1, 10):
                        self.toolbar.EnableTool(x, True)
                    for x in range(0, 5):
                        self.button[x].Enable(True)
                else:
                    self.toolbar.DeleteTool(11)
                    exit_ico = wx.Bitmap("png/redled32.png")
                    self.toolbar.AddTool(11, "Connected", exit_ico, "Target not found.")
                    self.toolbar.Realize()
                    self.lbldef.SetLabel(" AVR-HVP:Target not found.")

                    for x in range(0, 5):
                        self.button[x].Enable(False)

                self.portList.SetValue(nport)
            else:
                self.lbldef.SetLabel(" Board AVR-HVP. not found!!")
                self.toolbar.DeleteTool(11)
                exit_ico = wx.Bitmap("png/grayled32.png")
                self.toolbar.AddTool(11, "Not Connect", exit_ico, "Board AVR-HVP not Connect.")
                self.toolbar.Realize()
                self.portList.SetValue("PORT")

                for x in range(1, 10):
                    self.toolbar.EnableTool(x, False)
                for x in range(0, 5):
                    self.button[x].Enable(False)
        ser.close()

    def OnSync(self, e):
        lbldeftxt = ''
        for nport in self.serial_ports():
            try:
                ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
                ser.write(b'+getsig\n')
                time.sleep(0.2)
                x = ser.readline()
                if (len(x) > 6):
                    self.chipID = x[0:6]
                    if self.chipID != b'000000':
                        self.chipName = self.MCU_List[self.chipID.decode('utf-8')]

                        tar = self.chipName
                        targ = dict(self.Fuse_Conf[tar])
                        tarMem = dict(targ["Memory"])
                        tarFuse = dict(targ["Fuses"])

                        self.memSize = tarMem["FlashSize"]
                        self.pageSize = tarMem["FPageSize"]
                        fFuse = json.dumps(tarFuse, indent=2)
                        key = tarFuse.keys()

                        print(tarMem)
                        print(self.memSize)
                        print(self.pageSize)

                        ser.write(b'+chinfo\n')
                        time.sleep(0.2)
                        spx = ser.readall()

                        lfuse = spx[0:2]
                        hfuse = spx[2:4]
                        efuse = spx[4:6]
                        self.txtlFuse.Clear()
                        self.txtlFuse.AppendText(lfuse)
                        self.txthFuse.Clear()
                        self.txthFuse.AppendText(hfuse)
                        self.txteFuse.Clear()
                        self.txteFuse.AppendText(efuse)

                        self.toolbar.DeleteTool(11)
                        exit_ico = wx.Bitmap("png/greenled32.png")
                        self.toolbar.AddTool(11, "Connected", exit_ico, "Board AVR-HVP Connected.")
                        self.toolbar.Realize()
                        lbldeftxt = " Target >>> " + self.chipName

                        for x in range(1, 10):
                            self.toolbar.EnableTool(x, True)
                        for x in range(0, 5):
                            self.button[x].Enable(True)

                        self.featGen()

                    else:
                        self.toolbar.DeleteTool(11)
                        exit_ico = wx.Bitmap("png/redled32.png")
                        self.toolbar.AddTool(11, "Connected", exit_ico, "Target not found.")
                        self.toolbar.Realize()
                        lbldeftxt = " AVR-HVP:Target not found."

                        for x in range(1, 10):
                            self.toolbar.EnableTool(x, False)
                        for x in range(0, 5):
                            self.button[x].Enable(False)

                    ser.close()
                    self.portList.SetValue(nport)
                    break
                else:
                    lbldeftxt = " Board AVR-HVP. not found!!"
                    self.toolbar.DeleteTool(11)
                    exit_ico = wx.Bitmap("png/grayled32.png")
                    self.toolbar.AddTool(11, "Not Connect", exit_ico, "Board AVR-HVP not Connect.")
                    self.toolbar.Realize()
                    self.portList.SetValue("PORT")

                    for x in range(1, 10):
                        self.toolbar.EnableTool(x, False)
                    for x in range(0, 5):
                        self.button[x].Enable(False)

            except (OSError, TypeError):
                pass
                '''
                dlg = wx.MessageDialog(self, "\n\nCommunication Error.\n Check PORT and Try again...",
                                       "Error!", wx.OK)
                dlg.ShowModal()
                dlg.Destroy()
                '''
        self.lbldef.SetLabel(lbldeftxt)

    def OnUpload(self, e):
        print(self.HEXdata)
        if self.hexSize == '':
            dlg = wx.MessageDialog(self, "\n\nError!!\nLoad HEX and try again.",
                                   "Exit", wx.OK | wx.ICON_ERROR)
            if dlg.ShowModal() == wx.ID_OK:
                #dlg.Destroy()
                return
        dlg = wx.MessageDialog(self, "\nUpload Op-Code.\n\n================",
                               "Exit", wx.OK | wx.CANCEL | wx.ICON_INFORMATION)
        try:
            if dlg.ShowModal() == wx.ID_OK:
                tgflashsize = int(self.memSize) * 1024
                if tgflashsize < int(self.hexSize):
                    dlg = wx.MessageDialog(self, "\n\nError!!\nHEX over size.",
                                           "Exit", wx.OK | wx.ICON_ERROR)
                    if dlg.ShowModal() == wx.ID_OK:
                        #dlg.Destroy()
                        return

                shsize = ''
                if int(self.hexSize) >= 10000:
                    shsize = str(self.hexSize)
                if int(self.hexSize) < 10000:
                    shsize = '0' + str(self.hexSize)
                if int(self.hexSize) < 1000:
                    shsize = '00' + str(self.hexSize)
                if int(self.hexSize) < 100:
                    shsize = '000' + str(self.hexSize)
                if int(self.hexSize) < 10:
                    shsize = '0000' + str(self.hexSize)

                tl = self.txtCtrl.GetLineText(0).replace(" ", "")
                print("tl :" + tl)

                haddr = int(tl[3:7], 16)
                phaddr = str(haddr)
                if haddr < 1000:
                    phaddr = '0' + str(haddr)
                if haddr < 100:
                    phaddr = '00' + str(haddr)
                if haddr < 10:
                    phaddr = '000' + str(haddr)

                haddr = bytearray(phaddr, 'utf-8')
                psize = bytearray(self.pageSize, 'utf-8')
                hsize = bytearray(shsize, 'utf-8')
                nport = self.portList.GetValue()
                ser = serial.Serial(nport, self.pBUADRATE, timeout=0.5)
                ser.flush()

                print(b'+wrflsh' + hsize + psize + b'\n')

                if ser.isOpen():
                    ser.write(b'+wrflsh' + hsize + psize + b'\n')
                    time.sleep(0.2)
                    ackcmd = ser.readline()
                    ser.close()

                    print(ackcmd)
                    print(bytes(str(self.hexSize), 'utf-8'))

                    if ackcmd[:-1] == bytes(str(self.hexSize), 'utf-8'):
                        m = 0
                        for line in range(0, self.txtCtrl.GetNumberOfLines()-1):
                            tl = self.txtCtrl.GetLineText(line).replace(" ", "")
                            print(tl)
                            nn = int(tl[0:2], 16)
                            nnd = str(nn)
                            if nn < 10:
                                nnd = '0' + str(nn)
                            aa = int(tl[4:8], 16)
                            aad = str(aa)
                            if aa < 10000:
                                aad = '0' + str(aa)
                            if aa < 1000:
                                aad = '00' + str(aa)
                            if aa < 100:
                                aad = '000' + str(aa)
                            if aa < 10:
                                aad = '0000' + str(aa)
                            dd = tl[9:]
                            ddd = ''
                            j = 0
                            for dh in range(0, int(len(dd)/2)):
                                xxd = int(dd[j:j+2], 16)
                                xxi = str(xxd)
                                if xxd < 100:
                                    xxi = '0' + str(xxd)
                                if xxd < 10:
                                    xxi = '00' + str(xxd)
                                ddd += xxi
                                j += 2
                            cmph = nnd + aad + ddd + '\n'
                            ser = serial.Serial(nport, self.pBUADRATE, timeout=0.5)
                            ser.write(bytearray(cmph, 'utf-8'))
                            time.sleep(0.2)
                            ackcmd2 = ser.readall()
                            ser.close()
                            m += 1
                            print(str(m) + ' ' + str(ackcmd2))
                else:
                    pass
                #ser.close()
            else:
                pass
        finally:
            dlg.Destroy()

    def OnDownload(self, e):
        nport = self.portList.GetValue()
        ser = serial.Serial(nport, self.pBUADRATE, timeout=1)
        if ser.isOpen():
            ser.write(b'+rdflsh' + bytes(self.memSize,'utf-8') + b'\n')
            time.sleep(0.2)
            x1 = ser.readall()
            delt = x1.find(b'FFFFFFFF')
            if (delt % 2) != 0:
                delt += 1

            hdata = x1 #[:delt]
            cmphex = ''
            addrnh = ''
            nnha = ''
            addrn = 0
            dhex = ''
            fhex = ""
            for line in hdata.split(b'\r\n'):
                addrnh = '00000' + hex(addrn)[2:] + '0'
                if (len(hex(addrn)) > 1):
                    addrnh = '0000' + hex(addrn)[2:] + '0'
                if (len(hex(addrn)) > 2):
                    addrnh = '000' + hex(addrn)[2:] + '0'
                if (len(hex(addrn)) > 3):
                    addrnh = '00' + hex(addrn)[2:] + '0'
                if (len(hex(addrn)) > 4):
                    addrnh = '0' + hex(addrn)[2:] + '0'

                addrn += 1

                cmnh = str(line, 'utf-8')
                fhex += cmnh
                nh = len(line)/2
                nnh = hex(int(nh))[2:]
                sth = ''
                x = 0
                for i in range(0, int(nh)):
                    sth += ' ' + cmnh[x:x+2]
                    x += 2
                    dhex += sth
                nnha = nnh
                if int(nnh, 16) < 16:
                    nnha = '0' + nnh

                if nh != 0:
                    cmphex += nnha.upper() + ':' + addrnh.upper() + ':' + sth + '\n'

            hsize = int(addrnh, 16) + int(nnha, 16)
            self.hexSize = hsize

            delf = fhex.find('FFFFFFFF')
            self.HEXdata = fhex[:delf]

            #cmphex += '00:00000: 01 FF'
            crc16 = self.CRC16_from_code()
            self.txtCtrl.Clear()
            self.txtCtrl.AppendText(cmphex)
            self.txtCtrl.SetInsertionPoint(0)
            self.stbar.SetStatusText("CRC:" + crc16, 2)
            self.stbar.SetStatusText("Size: " + str(hsize) + " Bytes", 0)

        else:
            pass
        ser.close()

    def CRC16_from_file(self):
        buf = open(os.path.join(self.dirname, self.filename), 'rb').read()
        crc16 = crcmod.mkCrcFun(0x11021, 0x1d0f, False, 0x0000)
        crct = hex(crc16(buf))[2:].upper()
        return ("0x" + crct)

    def CRC16_from_code(self):
        buf = bytearray(self.HEXdata, 'utf-8')
        crc16 = crcmod.mkCrcFun(0x11021, 0xefef, False, 0x0000)
        crct = hex(crc16(buf))[2:].upper()
        return("0x" + crct)

    def onChecked(self, e):
        cb = e.GetEventObject()
        print(cb.GetLabel(), ' is clicked', cb.GetValue())

    def onTest(self, e):
        print("TEST..")
        print(self.chipID)
        fuse = self.fuseInfo[self.chipID]
        sfuse = sorted(fuse)
        mclk = int(sfuse[0],16) | (1<<7)
        print(sfuse)
        print(hex(mclk))
        self.sizer.Detach(self.txtBox)
        self.SetSizer(self.sizer)
        self.Show()
    
    def featGen(self) :
        with open('fuse_conf.json') as fuse_conf:
            self.Fuse_Conf = json.load(fuse_conf)
            
        tar = self.chipName
        targ = dict(self.Fuse_Conf[tar])

        tarFuse = dict(targ["Fuses"])
        
        fFuse = json.dumps(tarFuse, indent=2)
        key = tarFuse.keys()

        self.txtCtrl.Clear()
        self.txtCtrl.AppendText(fFuse)

app = wx.App(False)
frame = MainWindow(None, "nnF-AVR HVP v0.5.0")
app.MainLoop()
