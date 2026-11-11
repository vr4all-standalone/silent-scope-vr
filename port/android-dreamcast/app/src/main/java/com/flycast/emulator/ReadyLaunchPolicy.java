// SPDX-License-Identifier: GPL-2.0-or-later
package com.flycast.emulator;

/** UI-thread state for validated, once-only automatic entry and explicit Play. */
final class ReadyLaunchPolicy {
    private boolean resumed, consumed, launching, checking, ready, manual, importing;
    private int generation, checkToken=-1;

    ReadyLaunchPolicy(boolean suppressed, boolean restoredConsumed) {
        consumed = suppressed || restoredConsumed;
    }

    void onResume() { resumed = true; }
    void onPause() { resumed = false; manual = ready = false; invalidateCheck(); }
    void onNewEntry(boolean suppressed) {
        if (launching) return;
        consumed = suppressed;
        manual = ready = false;
        invalidateCheck();
    }
    boolean autoStartConsumed() { return consumed; }
    boolean isReady() { return ready && !checking && !launching && !importing; }
    boolean isImporting() { return importing; }
    private void invalidateCheck(){generation++;checkToken=-1;checking=false;}
    boolean beginImport(){if(importing||launching)return false;importing=true;ready=manual=false;invalidateCheck();return true;}
    void finishImport(){importing=false;ready=manual=false;invalidateCheck();}
    int beginCheck() {
        if (!resumed || checking || launching || importing) return -1;
        checking = true;
        ready = false;
        return checkToken=++generation;
    }
    boolean finishCheck(int token, boolean valid) {
        if (token != checkToken) return false;
        checking = false;checkToken=-1;
        ready = valid;
        if (!valid) manual = false;
        return true;
    }
    boolean requestPlay() {
        if (!resumed || !isReady()) return false;
        manual = true;
        return true;
    }
    boolean shouldLaunch() {
        if (!resumed || !isReady() || (consumed && !manual)) return false;
        consumed = true;
        manual = false;
        return true;
    }
    boolean beginLaunch() {
        if (!resumed || !isReady()) return false;
        consumed = launching = true;
        return true;
    }
    void launchFailed() { launching = false; consumed = true; }
}

