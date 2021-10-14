package com.aleksandar_damnjanovic.goodha_share;

import android.Manifest;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.view.ContextThemeWrapper;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.File;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity {

    final int READ_EXTERNAL_STORAGE_REQUEST=1;
    final int WRITE_EXTERNAL_STORAGE_REQUEST=2;
    final int MANAGE_EXTERNAL_STORAGE_REQUEST=3;

    boolean cancelMultiple= false;
    String temp_FileName="";
    int temp_order=0;
    int temp_count=0;
    long length=0;
    long transferred =-1;
    double av_speed=0;
    int counter=1;
    Dialog sender;
    Dialog detectDialog;
    TextView tv;
    boolean sending= false;
    Intent senderService;
    ListView localServices;
    public static Context mainContext;
    ArrayList<String> serviceList= null;
    String ip="";
    Activity activity;
    TextView default_destination_status;
    TextView detectDialogText;
    PopupMenu popup;
    public String HOST_FOLDER;
    SharedPreferences pp;
    SharedPreferences.Editor ee;
    TextView log;
    boolean recCanceled= false;

    static {
        System.loadLibrary("native-lib");
        System.loadLibrary("detector");
        System.loadLibrary("receiver");
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if(
        requestCode== READ_EXTERNAL_STORAGE_REQUEST ||
        requestCode== WRITE_EXTERNAL_STORAGE_REQUEST ||
        requestCode== MANAGE_EXTERNAL_STORAGE_REQUEST){
            if(grantResults.length>0 && grantResults[0]!= PackageManager.PERMISSION_GRANTED) {
            }else{
                Toast.makeText(this, "You must give all of requested permissions to " +
                        "Goodha Share in order" +
                        " to work properly", Toast.LENGTH_LONG).show();
                finish();
                System.exit(0);
            }
        }

    }

    private void CreateDialog(){
        sender=new Dialog(this);
        sender.setContentView(R.layout.sender_dialog);
        sender.setCancelable(false);
        tv= sender.findViewById(R.id.sample_text);
        tv.setText("Preparations.\n Please wait.");

        TextView sender_cancel= sender.findViewById(R.id.sender_cancel_button);
        sender_cancel.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                cancelMultiple= true;
                SenderService.running= false;
                stopService(senderService);
                cancelSending();
                sender.dismiss();
                String time= "";
                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                    DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss dd.MM.yyyy");
                    LocalDateTime now = LocalDateTime.now();
                    time= dtf.format(now);
                }
                final String tt= time;

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        applyToLog("Sending file "+ temp_FileName +" canceled to ip " + ip+" at " + tt);
                        readLog();
                    }
                });

            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if(requestCode==11 && resultCode==Activity.RESULT_OK)
            if(data!=null){

                if(data.getClipData()==null)
                    return;

                if(ip.equals(""))
                    Toast.makeText(getBaseContext(), "Default destination must be selected", Toast.LENGTH_LONG).show();

                int count= data.getClipData().getItemCount();
                if(count==1){
                    Uri uri= data.getData();
                    String path= RealPathUtil.getRealPath(this, uri);
                    sender.show();
                    send(this, path);
                }else if(count>1){
                    List<String>paths= new ArrayList<>();
                    for(int i = 0 ; i < count ; i++){
                        Uri ff= data.getClipData().getItemAt(i).getUri();
                        paths.add(RealPathUtil.getRealPath(getBaseContext(), ff));
                    }

                    cancelMultiple= false;
                    sendMultiple(getBaseContext(), paths);
                }
            }
    }

    @Override
    protected void onStart() {
        super.onStart();

        if(default_destination_status==null)
            default_destination_status= findViewById(R.id.default_destination_status);

        SharedPreferences prefs= this.getPreferences(MODE_PRIVATE);
        String default_dest= prefs.getString("default_destination", "");
        if(default_dest.equals(""))
            default_destination_status.setText("Default destination not set");
        else{
            default_destination_status.setText("Default destination set to "+default_dest);
        }
        ip= default_dest;

        if(SenderService.running){
            sender.show();
            final long time= System.currentTimeMillis();
            new Thread(new Runnable() {
                @Override
                public void run() {
                    while (SenderService.running)
                        readSendingState(time);

                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            sender.dismiss();
                            String time= "";
                            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                                DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss dd.MM.yyyy");
                                LocalDateTime now = LocalDateTime.now();
                                time= dtf.format(now);
                            }

                            applyToLog("File "+ temp_FileName +" sent to ip " + ip+" at " + time);
                            readLog();
                        }
                    });
                }
            }).start();

        }

        setHomeFolder();
        readLog();

    }

    private void setHomeFolder(){
        String pp= Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
                .getAbsolutePath();

        File ff=new File(pp, "GOODHA SHARE DOWNLOADS");

        if(!ff.exists())
            ff.mkdirs();

        HOST_FOLDER= ff.getAbsolutePath();
    }

    Intent receiverService;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        setHomeFolder();
        activity= this;
        log= findViewById(R.id.log);
        pp= activity.getPreferences(MODE_PRIVATE);
        ee= pp.edit();

        ImageView gear= (ImageView)activity.findViewById(R.id.bottomGear);
        gear.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                popup.show();
            }
        });

        if(ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)!=PackageManager.PERMISSION_GRANTED)
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, WRITE_EXTERNAL_STORAGE_REQUEST);

        if(ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)!=PackageManager.PERMISSION_GRANTED)
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, READ_EXTERNAL_STORAGE_REQUEST);

        Context wrapper = new ContextThemeWrapper(MainActivity.this, R.style.myPopupStyle);

        PopupMenu infoMenu= new PopupMenu(wrapper, gear);
        infoMenu.getMenuInflater().inflate(R.menu.info_menu, infoMenu.getMenu());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
            infoMenu.setForceShowIcon(true);

        ImageView infoButton= findViewById(R.id.infoButton);
        infoButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                infoMenu.show();
            }
        });

        popup = new PopupMenu(wrapper, gear);
        popup.getMenuInflater().inflate(R.menu.bottom_menu, popup.getMenu());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
            popup.setForceShowIcon(true);

        infoMenu.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem menuItem) {

                switch ((menuItem.getItemId())){

                    case R.id.OpenStorage:
                    {
                        Uri uri= Uri.parse(HOST_FOLDER);
                        Intent open= new Intent(Intent.ACTION_VIEW);
                        open.setDataAndType(uri, "resource/folder");
                        startActivity(open);

                        return true;
                    }

                    case R.id.OpenInfoPage:
                    {
                        Intent open= new Intent(Intent.ACTION_VIEW,
                                Uri.parse(getResources().getString(R.string.page)));
                        open.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        startActivity(open);

                        return true;
                    }

                }

                return false;
            }
        });

        popup.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem menuItem) {

                switch (menuItem.getItemId()){
                    case R.id.Load:
                    {
                        Intent intent =new Intent(Intent.ACTION_GET_CONTENT);
                        intent.setType("*/*");
                        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                        startActivityForResult(intent, 11);

                        return true;
                    }
                    case R.id.Scan:
                    {

                        detectDialog.show();
                        detectDialogText.setText("Scanning commenced");
                        detect();

                        new Thread(new Runnable() {
                            @Override
                            public void run() {

                                try {
                                    Thread.sleep(1000);
                                } catch (InterruptedException e) {
                                    e.printStackTrace();
                                }

                                while (getDetectionRunning()) {
                                    try {
                                        Thread.sleep(500);

                                        runOnUiThread(new Runnable() {
                                            @Override
                                            public void run() {
                                                double percent=(double)100/ (double) 65536;
                                                percent= percent * getScannedCount();

                                                detectDialogText.setText("Scanning...\n"
                                                        +String.format("%.2f", percent)+ " %\nof your local addresses scanned");
                                            }
                                        });

                                    } catch (InterruptedException e) {
                                        e.printStackTrace();
                                    }
                                }

                                runOnUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        detectDialog.dismiss();
                                    }
                                });

                                runOnUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        String result= getList();

                                        if(result.equals(""))
                                            return;

                                        String res[]= result.split("\\n");

                                        serviceList= new ArrayList<String>();
                                        for(String s: res)
                                            serviceList.add(s);

                                        ArrayAdapter adapter =new ArrayAdapter(getApplicationContext(), R.layout.single_element, serviceList);
                                        localServices.setAdapter(adapter);
                                        localServices.invalidate();

                                        localServices.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                                            @Override
                                            public void onItemClick(AdapterView<?> adapterView, View view, int i, long l) {
                                                for(int i1=0;i1< localServices.getCount();i1++)
                                                    localServices.getChildAt(i1).setBackground(ContextCompat.getDrawable(getBaseContext(), R.drawable.regular_element_shape));

                                                view.setBackground(ContextCompat.getDrawable(getBaseContext(), R.drawable.selected_element_shape));
                                                ip= serviceList.get(i).split("\\s")[0];

                                                SharedPreferences prefs= activity.getPreferences(MODE_PRIVATE);
                                                SharedPreferences.Editor editor= prefs.edit();
                                                editor.putString("default_destination", ip);
                                                editor.apply();
                                                default_destination_status.setText("Default destination set to "+ip);
                                            }
                                        });
                                    }
                                });

                            }
                        }).start();

                        return true;
                    }
                    case R.id.Exit:
                    {
                        stopService(receiverService);
                        finish();
                        System.exit(0);

                        return true;
                    }

                }

                return false;
            }
        });

        mainContext= this;
        String device = Build.DEVICE;
        senderService= new Intent(getBaseContext(), SenderService.class);
        receiverService= new Intent(this, ReceiverService.class);
        startService(receiverService);

        final Dialog receivingDialog= new Dialog(this);
        receivingDialog.setContentView(R.layout.sender_dialog);
        receivingDialog.setCancelable(false);
        final TextView receivingDialogText= receivingDialog.findViewById(R.id.sample_text);
        final TextView receivingCancelButton= receivingDialog.findViewById(R.id.sender_cancel_button);

        receivingCancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {

                        String time= "";
                        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                            DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss dd.MM.yyyy");
                            LocalDateTime now = LocalDateTime.now();
                            time= dtf.format(now);
                        }
                        final String tt= time;

                        recCanceled= true;
                        applyToLog("Receiving canceled at " + time);
                        readLog();
                        cancelReceiving();
                    }
                }).start();
            }
        });

        new Thread(new Runnable() {
            @Override
            public void run() {
                receiverStart(HOST_FOLDER);
            }
        }).start();

        new Thread(new Runnable() {
            @Override
            public void run() {
                while (true){
                    try {
                        Thread.sleep(500);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }

                    if(getPresentlyReceiving() && !receivingDialog.isShowing()){
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                receivingDialog.show();
                            }
                        });
                    }else if(!getPresentlyReceiving() && receivingDialog.isShowing()){
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                receivingDialog.dismiss();

                                if(!recCanceled){
                                    String time= "";
                                    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                                        DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss dd.MM.yyyy");
                                        LocalDateTime now = LocalDateTime.now();
                                        time= dtf.format(now);
                                    }
                                    final String tt= time;

                                    applyToLog("File "+ temp_FileName +" received from ip " + ip+" at " + tt);
                                    readLog();
                                }else
                                    recCanceled= false;

                            }
                        });
                    }else if(getPresentlyReceiving() && receivingDialog.isShowing()){
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                String result[]= getReceiverState().split("\\n");
                                String content="";

                                for(String s: result){
                                    String e[]= s.split("[|]{5}");
                                    String ip=e[0];
                                    String fn= e[1];
                                    String tr= parseByteString(Long.parseLong(e[2]));
                                    String sz= parseByteString(Long.parseLong(e[3]));

                                    content+="Receiving:\nFrom "+ip+"\nFile name: "+fn+"\n"+
                                            "transferred: "+tr+"\ntotal: "+sz+"\n\n";
                                }

                                receivingDialogText.setText(content);
                            }
                        });
                    }

                }
            }
        }).start();

        initializeDetector(device);
        CreateDialog();

        localServices= findViewById(R.id.local_services);
        detectDialog= new Dialog(this);
        detectDialog.setContentView(R.layout.sender_dialog);
        detectDialog.setCancelable(false);
        detectDialogText= detectDialog.findViewById(R.id.sample_text);

        TextView detectCancelButton= detectDialog.findViewById(R.id.sender_cancel_button);
        detectCancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                cancelScanning();
            }
        });

        ArrayList<String> list= new ArrayList<String>();
        ArrayAdapter adapter =new ArrayAdapter(this, R.layout.single_element, list);
        localServices.setAdapter(adapter);
        localServices.invalidate();

        Intent intent= getIntent();
        if(intent!=null){
            String action= intent.getAction();
            if(Intent.ACTION_SEND.equals(action)){
                Uri file= (Uri) intent.getParcelableExtra(Intent.EXTRA_STREAM);
                String path= RealPathUtil.getRealPath(this, file);
                sender.show();
                send(this, path);
            }else if(Intent.ACTION_SEND_MULTIPLE.equals(action)){
                List<Uri> file= intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
                List<String>paths= new ArrayList<>();
                for(int i=0;i<file.size();i++)
                    paths.add(RealPathUtil.getRealPath(getBaseContext(), file.get(i)));

                cancelMultiple= false;
                sendMultiple(getApplicationContext(), paths);

                }
            }
    }

    private String parseSeconds(long seconds){
        long min=seconds/60;
        long sec=seconds%60;
        if(min==0)
            return String.valueOf(sec)+" seconds";
        else
            return String.valueOf(min)+" minutes and "+ String.valueOf(sec)+" seconds";
    }

    private String parseByteString(long bytes){
        double res=0;
        String unit="";
        if(bytes>=(1024*1024*1024)){
            res=(double) bytes/(1024*1024*1024);
            unit=" GB";
        }else if(bytes>=(1024*1024)){
            res=(double) bytes/(1024*1024);
            unit=" MB";
        }else if(bytes>=(1024)) {
            res = (double) bytes / 1024;
            unit = " kB";
        }else{
            res = bytes;
            unit = " bytes";
        }

        return String.format("%.2f", res)+unit;
    }

    private void sendMultiple(Context context, List<String> files){

        String relativePath="/";

        new Thread(new Runnable() {
            @Override
            public void run() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        sender.show();
                    }
                });

                loop:for(int i=0;i<files.size();i++){

                    String file= files.get(i);
                    String name=file;
                    File ff= new File(name);
                    String fileName= ff.getName();

                    sending= true;
                    senderService.putExtra("ip", ip);
                    senderService.putExtra("name", fileName);
                    startService(senderService);

                    temp_FileName= fileName;
                    temp_order= i+1;
                    temp_count= files.size();
                    counter=1;
                    av_speed=0;
                    length=0;
                    transferred=-1;

                    if(!ip.equals(""))
                        send(ip, name, fileName, relativePath);
                    else{
                        SharedPreferences prefs= activity.getPreferences(MODE_PRIVATE);
                        String default_dest= prefs.getString("default_destination", "");
                        ip=default_dest;
                        if(!ip.equals(""))
                            send(ip, name, fileName, relativePath);
                        else
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    Toast.makeText(activity, "Destination ip not set", Toast.LENGTH_SHORT).show();
                                }
                            });
                    }

                    boolean test=true;
                    while (test==getRunning()) {
                        try {
                            Thread.sleep(250);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                    sending= false;

                    String time= "";
                    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                        DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss dd.MM.yyyy");
                        LocalDateTime now = LocalDateTime.now();
                        time= dtf.format(now);
                    }

                    applyToLog("File "+ temp_FileName +" sent to ip " + ip+" at " + time);

                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            readLog();
                        }
                    });

                    if(cancelMultiple)
                        break loop;

                }

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        sender.dismiss();
                    }
                });

                SenderService.running= false;
                stopService(senderService);
            }
        }).start();

        new Thread(new Runnable() {
            @Override
            public void run() {
                long time= System.currentTimeMillis();
                while(sending)
                    readSendingState(time);
            }
        }).start();

        temp_FileName="";
        temp_count=0;
        temp_order=0;
    }

    private void send(Context context, String file){

        length=0;
        transferred =-1;
        av_speed=0;
        counter=1;

        String name=file;
        File ff= new File(name);
        String fileName= ff.getName();
        String relativePath="/";

        temp_FileName= fileName;
        temp_order= 1;
        temp_count= 1;

        new Thread(new Runnable() {
            @Override
            public void run() {
                sending= true;
                senderService.putExtra("ip", ip);
                senderService.putExtra("name", fileName);
                startService(senderService);

                if(!ip.equals(""))
                    send(ip, name, fileName, relativePath);
                else{
                    SharedPreferences prefs= activity.getPreferences(MODE_PRIVATE);
                    String default_dest= prefs.getString("default_destination", "");
                    ip=default_dest;
                    if(!ip.equals(""))
                        send(ip, name, fileName, relativePath);
                    else
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                Toast.makeText(activity, "Destination ip not set", Toast.LENGTH_SHORT).show();
                            }
                        });
                }

                boolean test=true;
                while (test==getRunning()) {
                    try {
                        Thread.sleep(250);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
                sending= false;

            }
        }).start();

        final long time= System.currentTimeMillis();
        new Thread(new Runnable() {
            @Override
            public void run() {

                try {
                    Thread.sleep(500);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }

                while(sending){
                    readSendingState(time);
                }

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if(sender.isShowing()){

                            if(senderService!=null){
                                SenderService.running= false;
                                stopService(senderService);
                                if(sender.isShowing()){

                                    String time= "";
                                    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                                        DateTimeFormatter dtf = DateTimeFormatter.ofPattern("HH:mm:ss dd.MM.yyyy");
                                        LocalDateTime now = LocalDateTime.now();
                                        time= dtf.format(now);
                                    }

                                    applyToLog("File "+ fileName +" sent to ip " + ip+" at " + time);
                                    readLog();

                                    sender.dismiss();
                                }else
                                    sender.dismiss();
                            }else
                                sender.dismiss();

                        }

                    }
                });

            }
        }).start();

    }

    private void readSendingState(long time){
        try {
            Thread.sleep(250);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        length= getLength();
        transferred = getTransferred();

        String len=parseByteString(length);
        String tran= parseByteString(transferred);

        double speed= getSpeed();
        av_speed+= speed;

        String sp= String.format("%.2f", speed);
        String av_sp= String.format("%.2f", av_speed/counter);

        double tt= (length- transferred) / ((av_speed/counter)*(1024*1024));

        double percent= (double)transferred/(double)length;
        percent*=100;
        final double per= percent;
        String text1=tran+" of "+ len+"\n"+String.format("%.2f", per)+"% transferred";
        text1= text1+"\nSpeed: "+sp+" MB/s\nAverage speed: "+av_sp+" MB/s";
        text1+="\nRemaining time: "+ parseSeconds((int)tt);
        text1+="\nFile name: "+ temp_FileName;
        text1+="\nFiles: "+temp_order+" / "+temp_count;

        final String text=text1;

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if(length!=0)
                    if(per<100)
                        tv.setText(text);
            }
        });

        counter+=1;
    }

    private void applyToLog(String text){
        String ll= pp.getString("log", "");
        ll= text+"\n"+ll;
        ee.putString("log", ll);
        ee.apply();
    }

    private void readLog(){
        String ll= pp.getString("log", "");

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                log.setText(ll);
            }
        });

    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
    public native long getLength();
    public native long getTransferred();
    private native boolean getRunning();
    public native String bre(String bb);
    public native double getSpeed();
    public native void send(String ip, String localPath, String fileName, String relativePath);
    public native void cancelSending();

    /**
     * Native methods of detector library
     */
    public native void initializeDetector(String device);
    public native void detect();
    public native boolean getDetectionRunning();
    public native String getList();
    public native long getScannedCount();
    public native void cancelScanning();

    /**
     * Native methods of receiver library
     */
    public native void receiverStart(String relativePath);
    public native boolean getPresentlyReceiving();
    public native void cancelReceiving();
    public native String getReceiverState();

}