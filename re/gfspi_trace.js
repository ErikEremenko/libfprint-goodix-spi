/*
 * gfspi_trace.js  —  one-shot Frida trace of the Goodix Chicago SPI driver.
 *
 * Hooks BOTH sides of the SPI transport in gfspi.dll so we can see exactly what
 * Windows sends and receives around an image capture, with microsecond timing:
 *
 *   - SpiSendDataToDevice  (FUN_180066368, RVA 0x66368)
 *       int send(handle, u8 cmd0, u8 cmd1, void* payload, u16 len, u8 flag, int ackt)
 *       real command byte = (cmd0 << 4) | (cmd1 << 1)
 *   - data_from_device receive parser (RVA 0x67bbf) — logs the response cmd + len
 *
 * The point: our driver and Windows send byte-identical config/DAC/setmode, yet
 * Windows gets full frames and we get a row-31 cliff.  The difference must be in
 * the *timing* or an extra command around cmd 0x22 (image).  This trace shows it.
 *
 * Usage (admin PowerShell/cmd, secure boot off):
 *     frida -p <WUDFHost PID with gfspi.dll> -l gfspi_trace.js -o C:\gfspi_trace.log
 * Then trigger a few fingerprint scans (Windows Hello).  See the .md instructions.
 */

'use strict';

var MOD = 'gfspi.dll';
var RVA_SEND = 0x66368;      // SpiSendDataToDevice
var RVA_RECV = 0x67bbf;      // data_from_device (response parser)

/* high-resolution clock via QueryPerformanceCounter */
var QPC = new NativeFunction(Module.findExportByName('kernel32.dll', 'QueryPerformanceCounter'), 'int', ['pointer']);
var QPF = new NativeFunction(Module.findExportByName('kernel32.dll', 'QueryPerformanceFrequency'), 'int', ['pointer']);
var _qb = Memory.alloc(8);
QPF(_qb);
var FREQ = _qb.readU64().toNumber();
var lastUs = null;
function nowUs () { var b = Memory.alloc(8); QPC(b); return b.readU64().toNumber() / FREQ * 1e6; }
function stamp () {
  var t = nowUs();
  var d = (lastUs === null) ? 0 : (t - lastUs);
  lastUs = t;
  return '[' + (t / 1000).toFixed(3) + 'ms  +' + (d / 1000).toFixed(3) + 'ms]';
}
function hx (n, w) { var s = (n >>> 0).toString(16); while (s.length < w) s = '0' + s; return s; }
function bytesHex (ptr, len) {
  if (ptr.isNull() || len <= 0) return '';
  try {
    var buf = new Uint8Array(ptr.readByteArray(Math.min(len, 96)));
    var s = '';
    for (var i = 0; i < buf.length; i++) s += hx(buf[i], 2);
    return s + (len > 96 ? '..(' + len + ')' : '');
  } catch (e) { return '<unreadable len=' + len + '>'; }
}
function name (cmd) {
  var m = { 0x20: 'IMG_T0', 0x22: 'IMG_T1', 0x32: 'FDT_DOWN', 0x34: 'FDT_UP',
            0x36: 'FDT_BASE', 0x50: 'NAV', 0x70: 'SETMODE', 0x82: 'CHIPID',
            0x90: 'CONFIG', 0x98: 'DAC', 0xa2: 'RESET', 0xa6: 'OTP', 0xae: 'MCUSTATE' };
  return m[cmd] || '?';
}

function hookAll () {
  var base = Module.findBaseAddress(MOD);
  if (base === null) return false;
  console.log('[*] ' + MOD + ' @ ' + base + '  (send=' + base.add(RVA_SEND) + ' recv=' + base.add(RVA_RECV) + ')');

  /* ---- SEND ---- */
  Interceptor.attach(base.add(RVA_SEND), {
    onEnter: function (args) {
      var cmd0 = args[1].toInt32() & 0xff;
      var cmd1 = args[2].toInt32() & 0xff;
      var cmd  = ((cmd0 << 4) | (cmd1 << 1)) & 0xff;
      var len  = args[4].toInt32() & 0xffff;   // 5th arg (stack) — Frida resolves it
      var pl   = bytesHex(args[3], len);
      console.log(stamp() + ' TX cmd=0x' + hx(cmd, 2) + ' ' + name(cmd) +
                  ' len=' + len + ' payload=' + pl);
    }
  });

  /* ---- RECV (response parser) ---- log entry; we mainly want the timing/marker.
   * data_from_device's exact args vary, so just timestamp the arrival + dump the
   * first bytes of whatever buffer arg1 points at (best-effort). */
  Interceptor.attach(base.add(RVA_RECV), {
    onEnter: function (args) {
      var head = '';
      try { head = bytesHex(args[1], 24); } catch (e) { head = '?'; }
      console.log(stamp() + ' RX <response>  head=' + head);
    }
  });
  return true;
}

if (!hookAll()) {
  console.log('[!] ' + MOD + ' not loaded yet — waiting for it (trigger a fingerprint scan)…');
  var iv = setInterval(function () { if (hookAll()) { clearInterval(iv); } }, 400);
}
console.log('[*] gfspi_trace armed. Do a few fingerprint scans, then detach (Ctrl+D / q).');
