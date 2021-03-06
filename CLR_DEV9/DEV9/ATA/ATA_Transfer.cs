﻿using System.Collections.Concurrent;
using System.IO;
using System.Threading;

namespace CLRDEV9.DEV9.ATA
{
    [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1001:TypesThatOwnDisposableFieldsShouldBeDisposable")]
    partial class ATA_State
    {
        public bool dmaReady = false;
        public int nsector = 0; //sector count
        public int nsectorLeft = 0; //sectors left to transfer

        //Write Buffer(s)
        bool awaitFlush = false;
        byte[] currentWrite;
        long currentWriteSectors;

        ConcurrentQueue<byte[]> WriteCache = new ConcurrentQueue<byte[]>();
        ConcurrentQueue<long> WriteCacheSectors = new ConcurrentQueue<long>();

        Thread ioThread;
        ManualResetEvent ioWrite;
        ManualResetEvent ioRead;
        AutoResetEvent ioClose;
        Cmd waitingCmd = null;
        //Write Buffer(s)

        //Read Buffer
        int rdTransferred;
        int wrTransferred;
        //Max tranfer on 24bit is 256*512 = 128KB
        //Max tranfer on 48bit is 65536*512 = 32MB
        byte[] readBuffer = new byte[256 * 512];
        //Read Buffer

        //PIO Buffer
        int pioPtr;
        int pioEnd;
        byte[] pioBuffer = new byte[512];
        //PIO Buffer

        void IO_Thread()
        {
            WaitHandle[] ioWaits = new WaitHandle[] { ioRead, ioWrite };
            while (true)
            {
                int ioType = WaitHandle.WaitAny(ioWaits);
                //Read or Write
                if (ioType == 0)
                {
                    //Log_Info("ioRead");
                    IO_Read();
                }
                else if (ioType == 1)
                {
                    //Log_Info("ioWrite");
                    if (!IO_Write())
                    {
                        if (ioClose.WaitOne(0))
                        {
                            return;
                        }
                    }
                }
            }
        }
        
        void IO_Read()
        {
            long lba = HDD_GetLBA();

            if (lba == -1)
                throw new IOException("Invalid LBA");

            long pos = lba * 512L;
            hddImage.Seek(pos, SeekOrigin.Begin);

            if (hddImage.Read(readBuffer, 0, nsector * 512) != nsector * 512)
            {
                throw new IOException("Read Less Than Requested");
            }
            ioRead.Reset();
        }

        bool IO_Write()
        {
            if (!WriteCacheSectors.TryDequeue(out long sector))
            {
                ioWrite.Reset();
                return false;
            }
            WriteCache.TryDequeue(out byte[] data);
            hddImage.Seek(sector * 512L, SeekOrigin.Begin);
            hddImage.Write(data, 0, data.Length);
            hddImage.Flush();
            return true;
        }

        void HDD_ReadAsync(Cmd drqCMD)
        {
            ioWrite.Reset();

            nsectorLeft = 0;

            if (!HDD_CanAssessOrSetError()) return;

            nsectorLeft = nsector;
            if (readBuffer.Length < nsector * 512)
                readBuffer = new byte[nsector * 512];  
            waitingCmd = drqCMD;

            ioRead.Set();
        }
        //Test
        void HDD_ReadSync(Cmd drqCMD)
        {
            ioWrite.Reset();

            //wait until thread waiting
            while (ioThread.ThreadState != ThreadState.WaitSleepJoin & ioThread.ThreadState != ThreadState.Stopped)
            {
                System.Threading.Thread.Sleep(0);
            }

            nsectorLeft = 0;

            if (!HDD_CanAssessOrSetError()) return;

            nsectorLeft = nsector;
            if (readBuffer.Length < nsector * 512)
                readBuffer = new byte[nsector * 512];

            IO_Read();

            drqCMD();
        }

        bool HDD_CanAssessOrSetError()
        {
            if (!HDD_CanAccess(ref nsector))
            {
                //Read what we can
                regStatus |= (byte)DEV9Header.ATA_STAT_ERR;
                regError |= (byte)DEV9Header.ATA_ERR_ID;
                if (nsector == -1)
                {
                    PostCmdNoData();
                    return false;
                }
            }
            return true;
        }
        void HDD_SetErrorAtTransferEnd()
        {
            long currSect = HDD_GetLBA();
            currSect += nsector;
            if ((regStatus & DEV9Header.ATA_STAT_ERR) != 0)
            {
                //Error condition
                //Write errored sector to LBA
                currSect++;
                HDD_SetLBA(currSect);
            }
        }
    }
}
