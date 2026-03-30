// This file is distributed under a BSD license. See LICENSE.txt for details.

#include "_types.hpp"
#include "_start.hpp"
#include "_viruz2.hpp"
#include "_ogg.hpp"
#include "kdoc.hpp"
#include "kkriegergame.hpp"
#include "engine.hpp"
#include "genoverlay.hpp"
#include "rtmanager.hpp"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/****************************************************************************/

static CV2MPlayer *Sound;
static sMusicPlayer *Player = 0;
static sInt SoundTimer;
KDoc *Document;
KEnvironment *Environment;
KKriegerGame *Game;
sF32 GlobalFps;
extern sInt DebugTextMem;
sInt DebugTexMem = 0;

extern "C" sU8 DebugData[];
static sU8* PtrTable[] =
{
  (sU8 *) 0x54525450,     // entry 0: export data ('PTRT')
  (sU8 *) 0x454C4241,     // entry 1: viruzII tune ('ABLE')
};

extern sInt IntroLoop;

/****************************************************************************/

/****************************************************************************/

static sInt ForceRoot = -1;
static sInt ForceGameState = -1;
static FILE *RuntimeLog = 0;
static sInt NextRuntimeLogTime = 0;

static void RuntimeLogWrite(const sChar *fmt,...)
{
  if(!RuntimeLog)
    return;

  va_list ap;
  va_start(ap,fmt);
  vfprintf(RuntimeLog,fmt,ap);
  va_end(ap);
  fflush(RuntimeLog);
}

static void ParseDebugCmdLineOptions()
{
  const sChar *cmd = sSystem->GetCmdLine();
  const sChar *p;

  if(!cmd)
    return;

  p = sFindString(cmd,"--forceroot=");
  if(p)
    ForceRoot = sAtoi(p+12);

  p = sFindString(cmd,"--forcestate=");
  if(p)
    ForceGameState = sAtoi(p+13);
}

/****************************************************************************/

/****************************************************************************/

struct VFXEntry
{
  sU8 MajorId;
  sU8 MinorId;
  sS8 Gain;
  sU8 reserved;
  sU32 StartPos;
  sU32 EndPos;
  sU32 LoopSize;
};

static void RenderSoundEffects(KDoc *doc,sU8 *data)
{
  const sInt maxsamples = 0x10000;
  static sF32 smpf[maxsamples*2];
  static sS16 *smps = (sS16 *) smpf;
  sU32 *data32,size;
  sU8 *v2m;
  sInt i,j,count,start,len,fade;
  sF32 amp,ampx;
  VFXEntry *ent;

  sVERIFY(Sound);

  data32 = (sU32 *) data;
  sVERIFY(data32[0] == sMAKE4('V','F','X','0'));
  size = data32[1];
  v2m = (sU8 *) &data32[2];
  data32 = (sU32 *) (((sU8 *)data32)+size+12);
  count = *data32++;
  ent = (VFXEntry *) data32;

  if(Sound->Open(v2m))
  {
    for(i=0;i<count;i++)
    {
      start = ent->StartPos;
      if(ent->LoopSize)
        start = ent->EndPos - ent->LoopSize;
      len = sMin<sInt>(ent->EndPos - start,maxsamples);
      ampx = sFPow(10.0f,(ent->Gain / 5.0f) / 20.0f);

      Sound->Play(ent->StartPos);
      Sound->Render(smpf,len);
      fade = 1000;
      if(ent->MajorId & 1)
        fade = 44100;

      for(j=0;j<len;j++)
      {
        amp = ampx;
        if(j<fade)
          amp = amp*j/fade;
        if(j>len-fade)
          amp = amp*(len-j)/fade;

        smps[j*2+0] = sRange<sInt>(smpf[j*2+0]*amp*0x7fff,0x7fff,-0x7fff);
        smps[j*2+1] = sRange<sInt>(smpf[j*2+1]*amp*0x7fff,0x7fff,-0x7fff);
      }

      sSystem->SampleAdd(smps,len,4,ent->MinorId,!(ent->MajorId & 1));
      sSystem->Progress(doc->Ops.Count+i,doc->Ops.Count+count);
      ent++;
    }

    Sound->Close();
  }
}

/****************************************************************************/

void IntroSoundHandler(sS16 *stream,sInt left,void *user)
{
  static sF32 buffer[4096*2];
  sF32 *fp;
  sInt count;
  sInt i;
  CV2MPlayer *snd;

  SoundTimer += left;
  snd = Sound;
  if(snd)
  {
    while(left>0)
    {
      count = sMin<sInt>(4096,left);
      snd->Render(buffer,count);
      fp = buffer;
      for(i=0;i<count*2;i++)
        //*stream++ = 0;
        *stream++ = sRange<sInt>(*fp++*0x7fff,0x7fff,-0x7fff);
      left-=count;
    }
#pragma lekktor(off)
    if(SoundTimer>=(2*60+25)*44100)
    {
      if(Document && Document->SongData && Document->SongSize)
      {
        snd->Open(Document->SongData);
        snd->Play(0);
        SoundTimer = 0;
      }
    }
#pragma lekktor(on)
  }
  else
  {
    for(i=0;i<left*2;i++)
      *stream++ = 0;
    SoundTimer = 0;
  }
}

static void MusicPlayerHandler(sS16 *buffer,sInt samples,void *user)
{
  sMusicPlayer *player = (sMusicPlayer *) user;
  player->Render(buffer,samples);
}

extern sBool ConfigDialog(sInt nr);

sBool sAppHandler(sInt code,sDInt value)
{
  sInt beat;
  sU8 *data;
  sViewport vp,clearvp;
  sInt i,max;
  sF32 curfps;
  sU32 hdrFlags;
  static sF32 oldfps;
  static sInt FirstTime,ThisTime,LastTime,sample;
  static sInt framectr=0;
  
  KOp *root;

  //GenBitmapTextureSizeOffset = -1;
  switch(code)
  {
#if !sINTRO
  case sAPPCODE_CONFIG:
    sSetConfig(sSF_DIRECT3D|sSF_FULLSCREEN,800,600);
    break;
#endif

  case sAPPCODE_INIT:
    ParseDebugCmdLineOptions();
    RuntimeLog = fopen("player_kkrieger_runtime.log","wt");
    NextRuntimeLogTime = 0;
    RuntimeLogWrite("startup cmdline='%s'\n",sSystem->GetCmdLine() ? sSystem->GetCmdLine() : "");
    RuntimeLogWrite("forceroot=%d forcestate=%d\n",ForceRoot,ForceGameState);

    data = PtrTable[0];
    if(((sInt)data)==0x54525450)
    {
      auto TryLoadRuntime = [](const sChar *name) -> sU8 *
      {
        sU8 *p = sSystem->LoadFile(name);
        if(!p) return 0;
        if((*(sU32 *)p & ~7) == 0) return p;
        delete[] p;
        return 0;
      };

      const sChar *loadedName = 0;

      data = TryLoadRuntime(sSystem->GetCmdLine());
      if(data) loadedName = sSystem->GetCmdLine();
      if(data==0) { data = TryLoadRuntime("../../data/kkrieger3383.kx"); if(data) loadedName = "../../data/kkrieger3383.kx"; }
      if(data==0) { data = TryLoadRuntime("../data/kkrieger3383.kx"); if(data) loadedName = "../data/kkrieger3383.kx"; }
      if(data==0) { data = TryLoadRuntime("data/kkrieger3383.kx"); if(data) loadedName = "data/kkrieger3383.kx"; }
      if(data==0) { data = TryLoadRuntime("../../data/kkrieger.kx"); if(data) loadedName = "../../data/kkrieger.kx"; }
      if(data==0) { data = TryLoadRuntime("../data/kkrieger.kx"); if(data) loadedName = "../data/kkrieger.kx"; }
      if(data==0) { data = TryLoadRuntime("data/kkrieger.kx"); if(data) loadedName = "data/kkrieger.kx"; }
      // Keep exported runtime data as a fallback; local exports are often partial.
      if(data==0) { data = TryLoadRuntime("../../data/player_kkrieger_export.kx"); if(data) loadedName = "../../data/player_kkrieger_export.kx"; }
      if(data==0) { data = TryLoadRuntime("../data/player_kkrieger_export.kx"); if(data) loadedName = "../data/player_kkrieger_export.kx"; }
      if(data==0) { data = TryLoadRuntime("data/player_kkrieger_export.kx"); if(data) loadedName = "data/player_kkrieger_export.kx"; }
      if(data==0)
      {
        // Final fallback for developer/runtime builds: use data blob baked into kkrieger_data.asm.
        data = DebugData;
        loadedName = "<embedded DebugData>";
      }

      if(loadedName)
        sDPrintF("player_kkrieger data: %s\n",loadedName);
      RuntimeLogWrite("runtime_data='%s'\n",loadedName ? loadedName : "<none>");
    }

    hdrFlags = *(sU32 *)data;
    if(hdrFlags & ~7)
    {
      sSystem->Abort("invalid runtime data format (export .kx required)");
      return sFALSE;
    }

    Document = new KDoc;
    Document->Init(data);
    RuntimeLogWrite("song: bpm=%d size=%d sample=%d ops=%d\n",
      Document->SongBPM,Document->SongSize,Document->SampleSize,Document->Ops.Count);
    RuntimeLogWrite("roots after init:\n");
    for(i=0;i<MAX_OP_ROOT;i++)
    {
      KOp *r = Document->RootOps[i];
      sInt cls = (r && r->Cache) ? r->Cache->ClassId : -1;
      RuntimeLogWrite("  root[%d]=%p class=%d\n",i,r,cls);
    }
    Environment = new KEnvironment;
    Sound = 0;

    sInitPerlin();   
    GenOverlayInit();
    RenderTargetManager = new RenderTargetManager_;

    Engine = new Engine_;

    Game = new KKriegerGame;
    Game->Init();

    sFloatFix();
    i = sSystem->GetTime();
    Environment->Splines = &Document->Splines.Array;
    Environment->SplineCount = Document->Splines.Count;
    Environment->Game = Game;
    Environment->InitView();
    Environment->InitFrame(0,0);
    Document->Precalc(Environment);
    Environment->ExitFrame();

    if(Document->SongSize)
    {
      Sound = new CV2MPlayer;

      if(Document->SampleSize)
        RenderSoundEffects(Document,Document->SampleData);

      Sound->Open(Document->SongData);
      Sound->Play(0);
      SoundTimer = 0;
      sSystem->SetSoundHandler(IntroSoundHandler,64);
    }

    Game->ResetRoot(Environment,Document->RootOps[Document->CurrentRoot],1);
    if(ForceGameState>=0 && ForceGameState<256)
      Game->Switches[KGS_GAME] = ForceGameState;
    RuntimeLogWrite("after reset: currentRoot=%d gameState=%d\n",Document->CurrentRoot,Game->Switches[KGS_GAME]);

    FirstTime = sSystem->GetTime();
    LastTime = 0;
    oldfps = 0.0f;
    break;
#if !sINTRO || sPROJECT == sPROJ_SNOWBLIND
  case sAPPCODE_EXIT:
    sSystem->SetSoundHandler(0,0,0);
    if(Sound)
    {
      CV2MPlayer *old;
      old = Sound;
      Sound = 0;
      delete old;
    }
    delete Player;
    delete Environment;
    Game->Exit();
    delete Game;
    Document->Exit();
    delete Document;
    delete RenderTargetManager;
    delete Engine;
    GenOverlayExit();
    if(RuntimeLog)
    {
      RuntimeLogWrite("shutdown\n");
      fclose(RuntimeLog);
      RuntimeLog = 0;
    }
    break;
#endif
  case sAPPCODE_PAINT:
    // tick processing (moved up to reduce input lag by 1 frame)

    ThisTime = sSystem->GetTime() - FirstTime;

    beat = sMulDiv(ThisTime,Document->SongBPM,60000);

    curfps = ThisTime - LastTime;
    oldfps += (curfps - oldfps) * 0.1f;
    GlobalFps = 1000.0f / oldfps;

    max = ThisTime/10-LastTime/10;
    if(max>10) max = 10;

    sFloatFix();
    Game->OnTick(Environment,max);
    sSystem->Sample3DCommit();
    LastTime = ThisTime;

    sFloatFix();

    // root-switching logic and outermost stuff

    vp.Init();
    vp.Window.Init(0,0,sSystem->ConfigX,sSystem->ConfigY);
    GenOverlayManager->SetMasterViewport(vp);
    RenderTargetManager->SetMasterViewport(vp);

    sInt mode;
    mode = Game->GetNewRoot();
    if(ForceRoot>=0 && ForceRoot<MAX_OP_ROOT)
      mode = ForceRoot;

    if(mode<0 || mode>=MAX_OP_ROOT || !Document->RootOps[mode])
      mode = Document->CurrentRoot;

    if(mode!=Document->CurrentRoot)
    {
      sSystem->SetSoundHandler(0,0);
      Document->CurrentRoot = mode;
      Environment->InitView();
      Environment->InitFrame(0,0);
      Document->Precalc(Environment);
      Environment->ExitFrame();
      Game->ResetRoot(Environment,Document->RootOps[Document->CurrentRoot],0);

      if(Sound && Document->SongData && Document->SongSize)
      {
        Sound->Open(Document->SongData);
        Sound->Play(0);
        sSystem->SetSoundHandler(IntroSoundHandler,64);
      }
    }

    // timing

    Environment->InitFrame(beat,ThisTime);
    Document->AddEvents(Environment);
    Game->AddEvents(Environment);
    Game->FlushPhysic();

    // game painting

    clearvp.Init();
    sSystem->SetViewport(clearvp);
    sSystem->Clear(sVCF_ALL,0);

    Environment->GameCam.Init();
    Game->GetCamera(Environment->GameCam);
    Environment->GameCam.ZoomY = 1.0f*vp.Window.XSize()/vp.Window.YSize();
    Environment->Aspect = 1.0f*vp.Window.XSize()/vp.Window.YSize();

    if(Document->CurrentRoot<0 || Document->CurrentRoot>=MAX_OP_ROOT || !Document->RootOps[Document->CurrentRoot])
    {
      for(i=0;i<MAX_OP_ROOT;i++)
      {
        if(Document->RootOps[i])
        {
          Document->CurrentRoot = i;
          break;
        }
      }
    }

    root = Document->RootOps[Document->CurrentRoot];
    if(ThisTime>=NextRuntimeLogTime)
    {
      sInt rootClass = (root && root->Cache) ? root->Cache->ClassId : -1;
      RuntimeLogWrite("t=%d state=%d mode=%d curRoot=%d rootClass=%d\n",
        ThisTime,Game->Switches[KGS_GAME],mode,Document->CurrentRoot,rootClass);
      NextRuntimeLogTime = ThisTime + 500;
    }

    if(root)
    {
      GenOverlayManager->Reset(Environment);
      GenOverlayManager->RealPaint = sTRUE;
      GenOverlayManager->Game = Game;

      sFloatFix();
      root->Exec(Environment);

      GenOverlayManager->RealPaint = sFALSE;
      GenOverlayManager->Reset(Environment);
    }

    sFloatFix();
    Environment->ExitFrame();
    Environment->Mem.Flush();
//    sSystem->SetWinMouse(vp.Window.x1/2,vp.Window.y1/2);
    break;

  case sAPPCODE_KEY:
#if sPROJECT==sPROJ_KKRIEGER
    Game->OnKey(value);
    if(value == (sKEY_ESCAPE|sKEYQ_SHIFT) || value==sKEY_CLOSE)
      sSystem->Exit();
#else
    if(value == sKEY_ESCAPE)
      sSystem->Exit();
#endif
    break;
#if !sINTRO
  default:
    return sFALSE;
#endif
  }

  return sTRUE;
}
