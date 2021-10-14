package com.aleksandar_damnjanovic.goodha_share;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

public class ReceiverService extends Service {
    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    Notification notification=null;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Intent notificationIntent=new Intent(MainActivity.mainContext, MainActivity.class);
        PendingIntent pendingIntent=PendingIntent.getActivity(MainActivity.mainContext,1,
                notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        notification=new NotificationCompat.Builder(MainActivity.mainContext,
                App.CHANNEL_ID)
                .setContentTitle("Goodha Share")
                .setContentText("Service detectable")
                .setSmallIcon(R.drawable.goodha_detector)
                .setContentIntent(pendingIntent)
                .build();

        startForeground(2, notification);
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(STOP_FOREGROUND_REMOVE);
        }
    }


}