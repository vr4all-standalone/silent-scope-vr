"""Apply reviewed, bounded Silent Scope XR scope diagnostics/atomic presentation."""
from pathlib import Path
import difflib

root = Path('D:/VR4ALL-SilentScope-Build/flycast-source')
path = root / 'core/rend/vr/xr_host.cpp'
before = path.read_text(encoding='utf-8')
text = before

def replace(old, new):
    global text
    if text.count(old) != 1:
        raise RuntimeError('Expected exactly one current integration hunk: ' + old[:80])
    text = text.replace(old, new)

replace('double ssProfileStarted=-1,ssProfileLast=-1;',
        'double ssProfileStarted=-1,ssProfileLast=-1;\nuint64_t ssScopeFrames{},ssScopeReady{};')
replace('ssRates.reset();ssProfileStarted=ssProfileLast=-1;',
        'ssRates.reset();ssProfileStarted=ssProfileLast=-1;ssScopeFrames=ssScopeReady=0;')
replace('''	else
		rend_vr_drain(0);
	// What the panel says:''', '''	else
		rend_vr_drain(0);
    // The retained main/scope pair is now stable. Controller picking and both
    // eye draws must evaluate the same presentation time, even on a slow frame.
    ss::dc_scope::ScopedRifleDrawAuthorization scopePresentation(false);
	// What the panel says:''')
replace('''	bool ok = true;
	for (int i = 0; render && i < 2; i++)
	{
		EyeChain& c = chains[i];
        const bool drawRifle=silentScope&&focused&&ssRifleValid&&ssRifle&&!menuOpen&&!darkRoom;
        ss::dc_scope::ScopedRifleDrawAuthorization rifleAuthorization(drawRifle);''',
        '''	bool ok = true;
    const bool drawRifle=silentScope&&focused&&ssRifleValid&&ssRifle&&!menuOpen&&!darkRoom;
    // One scope freshness decision spans both eyes and the optic/native widget.
    ss::dc_scope::ScopedRifleDrawAuthorization rifleAuthorization(drawRifle);
	for (int i = 0; render && i < 2; i++)
	{
		EyeChain& c = chains[i];''')
replace('reinterpret_cast<uintptr_t>(GraphicsContext::Instance()),ss::dc_scope::monotonic_seconds(),&scope);',
        'reinterpret_cast<uintptr_t>(GraphicsContext::Instance()),ss::dc_scope::presentation_seconds(),&scope);\n            if(i==0){++ssScopeFrames;if(scope)++ssScopeReady;}')
replace('''                ssProfileLast=now;''', '''                const auto status=ss::dc_scope::gpu_scope().capture_status();
                NOTICE_LOG(RENDERER,"SS_SCOPE ready=%llu/%llu copies=%llu failures=%llu latest=%llu age_ms=%.1f gl=0x%x",
                    static_cast<unsigned long long>(ssScopeReady),static_cast<unsigned long long>(ssScopeFrames),
                    static_cast<unsigned long long>(status.copied),static_cast<unsigned long long>(status.failed),
                    static_cast<unsigned long long>(status.last_submit),status.last_seconds>=0?(now-status.last_seconds)*1000:-1.,
                    ss::dc_scope::gpu_scope().last_gl_error());
                ssScopeFrames=ssScopeReady=0;
                ssProfileLast=now;''')

patch = ''.join(difflib.unified_diff(before.splitlines(True), text.splitlines(True),
    fromfile='a/core/rend/vr/xr_host.cpp', tofile='b/core/rend/vr/xr_host.cpp'))
project = Path(__file__).resolve().parents[1]
(project / 'patches/dreamcast/0020-stereo-scope-presentation.patch').write_text(patch, encoding='utf-8')
path.write_text(text, encoding='utf-8', newline='')
print('Applied stereo scope presentation and bounded capture diagnostics')
