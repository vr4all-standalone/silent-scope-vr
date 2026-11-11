// SPDX-License-Identifier: GPL-2.0-or-later
package com.flycast.emulator;

import android.Manifest;
import android.app.Activity;
import android.content.*;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.graphics.drawable.*;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.text.*;
import android.text.style.ForegroundColorSpan;
import android.util.*;
import android.view.*;
import android.widget.*;
import java.io.*;

/** VR4ALL's preparation panel, backed by the exact owned Dreamcast disc validator. */
public final class SilentScopeStartActivity extends Activity {
    static final int DECK=0xFF0B0D12, PANEL=0xFF151923, LINE=0xFF343947,
            MUTED=0xFF9BA5B4, CREAM=0xFFF8F5EF, LIME=0xFFD9F36C, VIOLET=0xFFA99BFF;
    private static final int RAISED=0xFF1B202B, LIME_LIT=0xFFEDFFA6, COLUMN_DP=880;
    private static final int MATCH=LinearLayout.LayoutParams.MATCH_PARENT, WRAP=LinearLayout.LayoutParams.WRAP_CONTENT;
    private static final String STATE_CONSUMED="vr4all.autoStartConsumed";
    public static final String EXTRA_GAME="vr4all.silentScope.game", EXTRA_SUPPRESS_AUTO_START="vr4all.suppressAutoStart";
    private static final String FILE_HELP="Extract your owned Silent Scope (USA) Dreamcast disc first. Choose its CUE and all three BIN tracks together.\n\nThis version supports the verified US V1.000 layout. Disc files stay in this app's folder.";
    private static final String[] ARCHIVES=DreamcastFiles.NAMES;
    private ReadyLaunchPolicy launchPolicy;
    private TextView badgeText, message, accessStatus, foundAt, chevron;
    private View badgeDot;
    private LinearLayout badge, marksRow, more;
    private Button allow, play, again;
    private boolean moreTouched, resumed;
    private final Handler importHandler=new Handler(Looper.getMainLooper());
    private final Runnable importPoll=new Runnable(){@Override public void run(){
        if(!resumed||isFinishing()||isDestroyed())return;
        if(launchPolicy.isImporting()||DreamcastFiles.importInProgress()){
            show(false,true,true,"Importing your chosen disc files…");importHandler.postDelayed(this,250);
        }else check();
    }};

    private File downloads() {
        File folder=getExternalFilesDir("games");if(folder==null)folder=new File(getFilesDir(),"games");
        folder=DreamcastFiles.gameFolder(folder);
        if(!folder.isDirectory())folder.mkdirs();return folder;
    }
    private boolean suppressed(Intent intent) {
        return intent!=null&&(intent.getBooleanExtra("returned",false)
                || intent.getBooleanExtra("vr4all.suppressAutoStart",false));
    }
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        launchPolicy=new ReadyLaunchPolicy(suppressed(getIntent()),saved!=null && saved.getBoolean(STATE_CONSUMED));
        getWindow().setBackgroundDrawable(new ColorDrawable(DECK));
        LinearLayout page=new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setGravity(Gravity.CENTER_HORIZONTAL);
        page.setPadding(dp(40),dp(28),dp(40),dp(32));
        LinearLayout column=new Column(this,dp(COLUMN_DP));
        page.addView(column,new LinearLayout.LayoutParams(MATCH,WRAP));
        column.addView(topBar(),new LinearLayout.LayoutParams(MATCH,WRAP));
        View accent=new View(this);
        GradientDrawable bar=new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,new int[]{LIME,VIOLET,alpha(VIOLET,0)});
        bar.setCornerRadius(dp(2)); accent.setBackground(bar);
        column.addView(accent,sized(MATCH,dp(3),16));
        column.addView(headline("Your arcade.",CREAM),spaced(32));
        column.addView(headline("Inside the headset.",VIOLET),spaced(2));
        column.addView(panel(),sized(MATCH,WRAP,26));
        column.addView(inset(text("Your own original game files. Nothing of the game comes with this app.",17,MUTED,500)),spaced(18));
        column.addView(moreToggle(),spaced(6));
        column.addView(more=moreBody(),sized(MATCH,WRAP,6));
        column.addView(inset(text("Unofficial fan project, not affiliated with Konami.",14,MUTED,500)),spaced(32));
        ScrollView scroll=new ScrollView(this); scroll.setFillViewport(true);
        scroll.setBackground(glow()); scroll.addView(page); setContentView(scroll);
        if(saved!=null) { moreTouched=saved.getBoolean("moreTouched"); setMore(saved.getBoolean("moreOpen")); }
        show(false,canReadFiles(),false,suppressed(getIntent())?"VR session ended. Choose Play to enter again.":"Checking your game files…");
    }
    @Override public void onResume() {
        super.onResume();resumed=true;launchPolicy.onResume();importHandler.removeCallbacks(importPoll);importPoll.run();
    }
    @Override public void onPause() {
        resumed=false;importHandler.removeCallbacks(importPoll);launchPolicy.onPause();super.onPause();
    }
    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent); setIntent(intent);
        launchPolicy.onNewEntry(suppressed(intent)); check();
    }
    @Override protected void onSaveInstanceState(Bundle out) {
        out.putBoolean(STATE_CONSUMED,launchPolicy.autoStartConsumed());
        out.putBoolean("moreTouched",moreTouched);
        out.putBoolean("moreOpen",more.getVisibility()==View.VISIBLE);
        super.onSaveInstanceState(out);
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] results) {
        super.onRequestPermissionsResult(request,permissions,results);
        if(request==42) check();
    }
    private boolean canReadFiles() {
        return true;
    }
    private void askForAccess() {
        if(launchPolicy.isImporting()||DreamcastFiles.importInProgress())return;
        Intent select=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE).putExtra(Intent.EXTRA_ALLOW_MULTIPLE,true);
        try{startActivityForResult(select,43);}catch(RuntimeException unavailable){show(false,true,false,"Could not open the disc file chooser. Try again.");}
    }
    @Override protected void onActivityResult(int request,int result,Intent data) {
        super.onActivityResult(request,result,data);
        if(request!=43 || result!=RESULT_OK || data==null)return;
        final java.util.ArrayList<Uri> selected=new java.util.ArrayList<>();
        if(data.getClipData()!=null)for(int i=0;i<data.getClipData().getItemCount();i++)selected.add(data.getClipData().getItemAt(i).getUri());
        else if(data.getData()!=null)selected.add(data.getData());
        if(!launchPolicy.beginImport())return;
        show(false,true,true,"Importing your chosen disc files…");
        new Thread(()->{
            String error=null;
            try {
                if(selected.size()!=4)throw new IOException("Select the CUE and all three BIN tracks together");
                File root=downloads().getParentFile();
                if(root.getUsableSpace()<1250000000L)throw new IOException("Import needs at least1.25GB of free space for a complete private copy");
                try(DreamcastFiles.ImportSession importing=DreamcastFiles.beginImport(root)){
                    java.util.HashSet<String> names=new java.util.HashSet<>();
                    for(Uri uri:selected){
                        String name=null;
                        try(android.database.Cursor cursor=getContentResolver().query(uri,new String[]{android.provider.OpenableColumns.DISPLAY_NAME},null,null,null)){
                            if(cursor!=null && cursor.moveToFirst())name=cursor.getString(0);
                        }
                        if(name==null || !names.add(name))throw new IOException("Missing or duplicate disc filename");
                        try(InputStream input=getContentResolver().openInputStream(uri)){
                            if(input==null)throw new IOException("Cannot read the chosen disc file");
                            DreamcastFiles.copyTrack(input,importing.stagingFolder(),name);
                        }
                    }
                    importing.commit();
                }
            }catch(Exception failure){error=failure.getMessage()!=null?failure.getMessage():failure.toString();}
            final String failure=error;
            runOnUiThread(()->{
                launchPolicy.finishImport();if(!resumed||isFinishing()||isDestroyed())return;
                importHandler.removeCallbacks(importPoll);
                if(failure==null)check();else show(false,true,false,"Could not import disc: "+failure);
            });
        },"Silent Scope disc import").start();
    }
    private void check() {
        if(isFinishing()||isDestroyed())return;
        if(launchPolicy.isImporting()||DreamcastFiles.importInProgress()){
            show(false,true,true,"Importing your chosen disc files…");importHandler.removeCallbacks(importPoll);importHandler.postDelayed(importPoll,250);return;
        }
        final int token=launchPolicy.beginCheck();
        if(token<0 || isFinishing() || isDestroyed()) return;
        final boolean access=canReadFiles();
        if(!access) {
            launchPolicy.finishCheck(token,false);
            show(false,false,false,"Allow file access below so the app can check your Download folder.");
            return;
        }
        show(false,true,true,"Checking your Silent Scope disc…");
        new Thread(()->{
            String error=null;
            try {
                DreamcastFiles.validate(downloads());
                SilentScopeConfig.prepare(this);
            } catch(Exception exception) { error=exception.getMessage()!=null?exception.getMessage():exception.toString(); }
            final String result=error;
            runOnUiThread(()->{
                if(!resumed || isFinishing() || isDestroyed()) return;
                if(!launchPolicy.finishCheck(token,result==null)) { check(); return; }
                show(result==null,true,false,result!=null?"Game files are not ready:\n"+result:
                        (launchPolicy.autoStartConsumed()?"Your files are ready. Choose Play to enter VR.":"Your files are ready. Starting VR…"));
                if(launchPolicy.shouldLaunch()) launch();
            });
        },"Silent Scope file check").start();
    }
    private void play() {
        if(launchPolicy.requestPlay()) check();
    }
    private void launch() {
        if(isFinishing() || isDestroyed() || !canReadFiles() || !launchPolicy.beginLaunch()) return;
        Intent intent=new Intent(Intent.ACTION_MAIN).setClass(this,NativeGLActivity.class);
        intent.putExtra(EXTRA_GAME,new File(downloads(),DreamcastFiles.NAMES[0]).getAbsolutePath());
        intent.addCategory("com.oculus.intent.category.VR");
        intent.addCategory("org.khronos.openxr.intent.category.IMMERSIVE_HMD");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try { startActivity(intent); finishAndRemoveTask(); }
        catch(RuntimeException error) {
            launchPolicy.launchFailed();
            show(true,true,false,"Could not open VR: "+(error.getMessage()!=null?error.getMessage():error.toString())+". Choose Play to try again.");
        }
    }

    private View topBar() {
        LinearLayout bar=row();
        TextView tile=text("SS",17,DECK,800); tile.setGravity(Gravity.CENTER);
        tile.setIncludeFontPadding(false); tile.setBackground(shape(VIOLET,0,corners(13,5)));
        bar.addView(tile,new LinearLayout.LayoutParams(dp(46),dp(46)));
        LinearLayout names=new LinearLayout(this); names.setOrientation(LinearLayout.VERTICAL);
        TextView brand=text("VR4ALL",11,VIOLET,800); brand.setLetterSpacing(0.16f);
        names.addView(brand); names.addView(text("Silent Scope VR",21,CREAM,700));
        LinearLayout.LayoutParams grow=new LinearLayout.LayoutParams(0,WRAP,1); grow.leftMargin=dp(16);
        bar.addView(names,grow);
        String version=version();
        if(!version.isEmpty()) {
            TextView pill=text(version,15,MUTED,600); pill.setPadding(dp(12),dp(5),dp(12),dp(5));
            pill.setBackground(shape(0,LINE,round(20))); bar.addView(pill);
        }
        return bar;
    }
    private View panel() {
        LinearLayout panel=box(), head=row();
        TextView kind=text("ARCADE · RAIL SHOOTER",14,MUTED,700); kind.setLetterSpacing(0.14f);
        head.addView(kind,new LinearLayout.LayoutParams(0,WRAP,1));
        badge=row(); badge.setPadding(dp(14),dp(7),dp(16),dp(7)); badgeDot=new View(this);
        badge.addView(badgeDot,new LinearLayout.LayoutParams(dp(9),dp(9)));
        badgeText=text("",15,VIOLET,700); badgeText.setLetterSpacing(0.1f);
        LinearLayout.LayoutParams gap=new LinearLayout.LayoutParams(WRAP,WRAP); gap.leftMargin=dp(9);
        badge.addView(badgeText,gap); head.addView(badge); panel.addView(head,new LinearLayout.LayoutParams(MATCH,WRAP));
        marksRow=new LinearLayout(this);
        marksRow.setOrientation(narrow()?LinearLayout.VERTICAL:LinearLayout.HORIZONTAL);
        panel.addView(marksRow,spaced(18));
        message=text("",17,CREAM,500); panel.addView(message,sized(MATCH,WRAP,18));
        play=new Button(this); play.setText("▶︎  Play");
        style(play,22,DECK,shape(LIME,0xFFEAFF9D,corners(12,5)),shape(LIME_LIT,0xFFEAFF9D,corners(12,5)));
        play.setPadding(dp(44),dp(14),dp(48),dp(14)); play.setMinWidth(dp(240));
        play.setOnClickListener(v->play()); panel.addView(play,spaced(24));
        return panel;
    }
    private View moreToggle() {
        LinearLayout toggle=row(); toggle.setPadding(dp(4),dp(10),dp(12),dp(10));
        toggle.setBackground(pressable(shape(0,0,round(10)),shape(alpha(CREAM,0x12),0,round(10))));
        toggle.addView(text("Where to put your files",18,CREAM,600));
        chevron=text("›",26,VIOLET,700); chevron.setGravity(Gravity.CENTER); chevron.setIncludeFontPadding(false);
        LinearLayout.LayoutParams gap=new LinearLayout.LayoutParams(dp(28),dp(28)); gap.leftMargin=dp(8);
        toggle.addView(chevron,gap); toggle.setOnClickListener(v->{ moreTouched=true; setMore(more.getVisibility()!=View.VISIBLE); });
        return toggle;
    }
    private LinearLayout moreBody() {
        LinearLayout body=box(); body.addView(step("01","Choose your disc files"));
        accessStatus=text("",17,MUTED,500); body.addView(accessStatus,spaced(8));
        allow=button("Choose disc files",VIOLET,DECK,0xFFC4B9FF,0); allow.setOnClickListener(v->askForAccess());
        body.addView(allow,spaced(14));
        View rule=new View(this); rule.setBackgroundColor(LINE); body.addView(rule,sized(MATCH,dp(1),24));
        body.addView(step("02","Put the game on the headset"),spaced(22));
        body.addView(text("Use Choose disc files to select the CUE and three BIN files from your owned Dreamcast disc. The app keeps its working copy and saved state in its own folder.",17,MUTED,500),spaced(8));
        body.addView(text(FILE_HELP,17,CREAM,500),spaced(10));
        foundAt=text("",17,MUTED,500); body.addView(foundAt,spaced(10));
        again=button("Look again",RAISED,CREAM,0xFF252B37,0xFF464D55);
        again.setOnClickListener(v->check()); body.addView(again,spaced(16));
        return body;
    }
    private void setMore(boolean open) { more.setVisibility(open?View.VISIBLE:View.GONE); chevron.setRotation(open?90:0); }
    private void show(boolean ready,boolean access,boolean busy,String detail) {
        int tone=ready?LIME:VIOLET;
        badgeText.setText(ready?"READY":busy?"CHECKING":access?"FILES NEEDED":"ACCESS NEEDED");
        badgeText.setTextColor(tone);
        GradientDrawable dot=new GradientDrawable(); dot.setShape(GradientDrawable.OVAL); dot.setColor(tone);
        badgeDot.setBackground(dot); badge.setBackground(shape(alpha(tone,0x1F),alpha(tone,0x73),round(20)));
        message.setText(detail); message.setTextColor(ready?CREAM:VIOLET);
        play.setEnabled(ready && launchPolicy.isReady()); play.setAlpha(play.isEnabled()?1f:0.35f);
        allow.setEnabled(!busy);again.setEnabled(!busy);
        marksRow.removeAllViews();
        if(access) for(int i=0;i<ARCHIVES.length;i++) {
            String name=ARCHIVES[i]; boolean present=new File(downloads(),name).isFile();
            String suffix=ready?(present?" ✓":" included ✓"):(present?(busy?" checking":" found"):" missing");
            LinearLayout.LayoutParams gap=new LinearLayout.LayoutParams(WRAP,WRAP);
            if(i>0) { if(marksRow.getOrientation()==LinearLayout.HORIZONTAL) gap.leftMargin=dp(10); else gap.topMargin=dp(8); }
            marksRow.addView(mark(name+suffix,ready),gap);
        }
        marksRow.setVisibility(access?View.VISIBLE:View.GONE);
        accessStatus.setText("Your game files stay in this app's own folder. No all-files permission is needed.");
        accessStatus.setTextColor(MUTED); allow.setVisibility(View.VISIBLE);
        foundAt.setText(access?"Checking: "+shortPath(downloads()):"");
        foundAt.setVisibility(access?View.VISIBLE:View.GONE);
        if(!moreTouched) setMore(!ready && !busy);
    }
    private TextView mark(String label,boolean valid) {
        SpannableString value=new SpannableString(label);
        if(valid) value.setSpan(new ForegroundColorSpan(LIME),label.length()-1,label.length(),Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        TextView t=text(value,18,valid?CREAM:VIOLET,600); t.setPadding(dp(14),dp(8),dp(16),dp(8));
        t.setBackground(valid?shape(RAISED,LINE,round(10)):shape(alpha(VIOLET,0x1F),alpha(VIOLET,0x80),round(10)));
        return t;
    }
    private String version() {
        try {
            String name=getPackageManager().getPackageInfo(getPackageName(),0).versionName;
            if(name==null || name.isEmpty()) return "";
            int dash=name.indexOf('-'); return "v"+(dash>=0?name.substring(0,dash):name);
        } catch(PackageManager.NameNotFoundException error) { return ""; }
    }
    private String shortPath(File file) {
        String path=file.getAbsolutePath(), root=Environment.getExternalStorageDirectory().getAbsolutePath();
        return path.startsWith(root+"/")?path.substring(root.length()+1):file.getName();
    }
    /** The page's background: the deck, a violet glow top right and a dim teal one bottom left. */
    private Drawable glow()
    {
        return new LayerDrawable(new Drawable[] { new ColorDrawable(DECK),
                radial(alpha(VIOLET, 0x26), 0.74f, 0f, 560), radial(0x40253D3B, 0.12f, 1f, 480) });
    }

    private GradientDrawable radial(int colour, float x, float y, int radiusDp)
    {
        GradientDrawable g = new GradientDrawable();
        g.setGradientType(GradientDrawable.RADIAL_GRADIENT);
        g.setColors(new int[] { colour, alpha(colour, 0) });
        g.setGradientCenter(x, y);
        g.setGradientRadius(dp(radiusDp));
        return g;
    }

    private TextView headline(String s, int colour)
    {
        TextView t = text(s, 38, colour, 800);
        t.setLetterSpacing(-0.02f);
        t.setLineSpacing(0, 1f);
        t.setIncludeFontPadding(false);
        return t;
    }

    /** A step's heading in the steps' box: its number in lime, then its title. */
    private View step(String number, String title)
    {
        LinearLayout r = row();
        TextView n = text(number, 15, LIME, 800);
        n.setLetterSpacing(0.08f);
        r.addView(n);
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(WRAP, WRAP);
        p.leftMargin = dp(12);
        r.addView(text(title, 19, CREAM, 700), p);
        return r;
    }

    private TextView text(CharSequence s, float sp, int colour, int weight)
    {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(colour);
        t.setTypeface(Typeface.create(Typeface.SANS_SERIF, weight, false));
        t.setLineSpacing(0, 1.2f);
        return t;
    }

    /** Text lined up with the panels' rounded edge. */
    private TextView inset(TextView t)
    {
        t.setPadding(dp(4), 0, dp(4), 0);
        return t;
    }

    private Button button(String label, int fill, int textColour, int pressedFill, int stroke)
    {
        Button b = new Button(this);
        b.setText(label);
        style(b, 18, textColour, shape(fill, stroke, round(10)), shape(pressedFill, stroke, round(10)));
        b.setPadding(dp(28), dp(11), dp(28), dp(11));
        return b;
    }

    private void style(Button b, float sp, int textColour, Drawable normal, Drawable lit)
    {
        b.setAllCaps(false);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        b.setTextColor(textColour);
        b.setTypeface(Typeface.create(Typeface.SANS_SERIF, 700, false));
        b.setGravity(Gravity.CENTER);
        b.setStateListAnimator(null);
        b.setBackground(pressable(normal, lit));
    }

    /** Lit while pressed or under the headset's pointer. */
    private static StateListDrawable pressable(Drawable normal, Drawable lit)
    {
        StateListDrawable d = new StateListDrawable();
        d.addState(new int[] { android.R.attr.state_pressed }, lit);
        d.addState(new int[] { android.R.attr.state_hovered }, lit);
        d.addState(new int[0], normal);
        return d;
    }

    /** A panel: the panel colour, a 16 dp radius and a 1 dp line border, padded. */
    private LinearLayout box()
    {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(28), dp(24), dp(28), dp(26));
        box.setBackground(shape(PANEL, LINE, round(16)));
        return box;
    }

    private LinearLayout row()
    {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER_VERTICAL);
        return r;
    }

    /** A rounded shape: fill (0: none), a 1 dp stroke (0: none), corner radii in dp. */
    private GradientDrawable shape(int fill, int stroke, float[] radiiDp)
    {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        if (stroke != 0)
            g.setStroke(dp(1), stroke);
        float[] radii = new float[8];
        for (int i = 0; i < 8; i++)
            radii[i] = dp(radiiDp[i]);
        g.setCornerRadii(radii);
        return g;
    }

    private static float[] round(float r)
    {
        return new float[] { r, r, r, r, r, r, r, r };
    }

    /** VR4All's cut corners: top left and bottom right round by a, the other two by b. */
    private static float[] corners(float a, float b)
    {
        return new float[] { a, a, b, b, a, a, b, b };
    }

    private static int alpha(int colour, int a)
    {
        return (colour & 0x00FFFFFF) | (a << 24);
    }

    /** The file marks go one under another when the column can't fit three side by side. */
    private boolean narrow()
    {
        DisplayMetrics m = getResources().getDisplayMetrics();
        float width = Math.min(m.widthPixels / m.density - 80, COLUMN_DP);
        return width - 56 < 600;
    }

    private LinearLayout.LayoutParams spaced(int top)
    {
        return sized(WRAP, WRAP, top);
    }

    private LinearLayout.LayoutParams sized(int width, int height, int top)
    {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(width, height);
        p.topMargin = dp(top);
        return p;
    }

    private int dp(float v)
    {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    /** A vertical column no wider than maxWidth (px); its parent centres it. */
    private static final class Column extends LinearLayout
    {
        private final int maxWidth;

        Column(Context context, int maxWidth)
        {
            super(context);
            this.maxWidth = maxWidth;
            setOrientation(VERTICAL);
        }

        @Override
        protected void onMeasure(int widthSpec, int heightSpec)
        {
            if (MeasureSpec.getMode(widthSpec) != MeasureSpec.UNSPECIFIED && MeasureSpec.getSize(widthSpec) > maxWidth)
                widthSpec = MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.EXACTLY);
            super.onMeasure(widthSpec, heightSpec);
        }
    }
}

