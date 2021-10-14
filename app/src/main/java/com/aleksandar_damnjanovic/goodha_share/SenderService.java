package com.aleksandar_damnjanovic.goodha_share;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

public class SenderService extends Service {
    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    Notification notification=null;
    public static boolean running=false;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {

        running= true;
        String ip= intent.getStringExtra("ip");
        String name= intent.getStringExtra("name");

        Intent notificationIntent=new Intent(MainActivity.mainContext, MainActivity.class);
        PendingIntent pendingIntent=PendingIntent.getActivity(MainActivity.mainContext,0,
                notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        notification=new NotificationCompat.Builder(MainActivity.mainContext,
                App.CHANNEL_ID)
                .setContentTitle("Goodha Share")
                .setContentText("Sending "+ name+" to "+ip)
                .setSmallIcon(R.drawable.goodha_sending)
                .setContentIntent(pendingIntent)
                .build();

        startForeground(1, notification);
        return START_NOT_STICKY;
    }



    @Override
    public void onDestroy() {
        running= false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(STOP_FOREGROUND_REMOVE);
        }
    }

}