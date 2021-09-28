/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package com.android.framebufferizer;


import com.android.framebufferizer.utils.FrameBufferBuffer;

import java.awt.*;
import java.util.Random;

import javax.swing.*;

public class Main {
    static private FrameBufferBuffer theFramebuffer;
    static private JFrame theFrame;
    static private Random random;

    private static void createAndShowUI() {
        theFrame = new JFrame("TeeuiFramebufferizer");
        theFrame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        theFramebuffer = new FrameBufferBuffer();
        theFramebuffer.setFrame(theFrame);
        FrameBufferBuffer.MagnifiedView magnifiedView = theFramebuffer.getMagnifiedView();
        FrameBufferBuffer.ConfigSelector configSelector = theFramebuffer.getConfigSelector();
        magnifiedView.setPreferredSize(new Dimension(100, 100));
        magnifiedView.setMinimumSize(new Dimension(100, 100));
        magnifiedView.setMaximumSize(new Dimension(100, 100));
        theFrame.getContentPane().add(magnifiedView, BorderLayout.EAST);
        theFrame.getContentPane().add(theFramebuffer, BorderLayout.CENTER);
        theFrame.getContentPane().add(configSelector, BorderLayout.NORTH);
        theFrame.pack();
        theFrame.setVisible(true);
    }

    public static void main(String[] args) {
        random = new Random();
        javax.swing.SwingUtilities.invokeLater(new Runnable() {
            @Override
            public void run() {
                createAndShowUI();
            }
        });
    }
}
