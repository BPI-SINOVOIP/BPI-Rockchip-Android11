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

package com.android.framebufferizer.utils;

import com.android.framebufferizer.NativeRenderer;

import java.awt.*;
import java.awt.event.ComponentEvent;
import java.awt.event.ComponentListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseMotionListener;
import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;
import java.awt.geom.AffineTransform;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.DataBufferInt;
import java.awt.image.DirectColorModel;
import java.awt.image.Raster;
import java.awt.image.RenderedImage;
import java.awt.image.WritableRaster;

import javax.swing.*;


public class FrameBufferBuffer extends JPanel implements ComponentListener, MouseMotionListener {
    public class MagnifiedView extends JPanel implements ComponentListener {
        private BufferedImage mImage;

        protected MagnifiedView() {
            addComponentListener(this);
        }

        @Override
        public void componentResized(ComponentEvent e) {
            synchronized (this) {
                mImage = new BufferedImage(getWidth(), getHeight(), BufferedImage.TYPE_INT_ARGB_PRE);
            }
        }

        @Override
        public void componentMoved(ComponentEvent e) {

        }

        @Override
        public void componentShown(ComponentEvent e) {

        }

        @Override
        public void componentHidden(ComponentEvent e) {

        }

        public int d() { return getWidth()/ 5; }

        public void draw(RenderedImage image) {
            if (mImage == null) return;
            Graphics2D gc = mImage.createGraphics();
            gc.drawRenderedImage(image, AffineTransform.getScaleInstance(5.0, 5.0));
            repaint();
        }

        @Override
        public void paint(Graphics g) {
            synchronized (this) {
                g.drawImage(mImage, 0, 0, null);
            }
        }

    }


    public class ConfigSelector extends JPanel implements ActionListener {
        private final String languages [];

        {
            languages = NativeRenderer.getLanguageIdList();
        }

        private JComboBox<String> deviceSelector = new JComboBox(DeviceInfoDB.Device.values());
        private JCheckBox magnifiedCheckbox = new JCheckBox("Magnified");
        private JCheckBox invertedCheckbox = new JCheckBox("Inverted");
        private JComboBox<String> localeSelector = new JComboBox(languages);
        private JTextField confirmationMessage = new JTextField();

        protected ConfigSelector() {
            System.err.println("ConfigSelector");
            this.setLayout(new GridBagLayout());

            GridBagConstraints c = null;

            c = new GridBagConstraints();
            c.gridx = 0;
            c.gridy = 0;
            this.add(new JLabel("Select Device:"), c);

            deviceSelector.addActionListener(this);
            c = new GridBagConstraints();
            c.gridx = 1;
            c.gridy = 0;
            c.fill = GridBagConstraints.HORIZONTAL;
            c.gridwidth = 2;
            this.add(deviceSelector, c);

            c = new GridBagConstraints();
            c.gridx = 0;
            c.gridy = 1;
            this.add(new JLabel("Select Locale:"), c);

            localeSelector.addActionListener(this);
            c = new GridBagConstraints();
            c.gridx = 1;
            c.gridy = 1;
            c.gridwidth = 2;
            c.fill = GridBagConstraints.HORIZONTAL;
            this.add(localeSelector, c);

            c = new GridBagConstraints();
            c.gridx = 0;
            c.gridy = 2;
            this.add(new JLabel("UIOptions:"), c);

            magnifiedCheckbox.addActionListener(this);
            c = new GridBagConstraints();
            c.gridx = 1;
            c.gridy = 2;
            this.add(magnifiedCheckbox, c);

            invertedCheckbox.addActionListener(this);
            c = new GridBagConstraints();
            c.gridx = 2;
            c.gridy = 2;
            this.add(invertedCheckbox, c);

            c = new GridBagConstraints();
            c.gridx = 0;
            c.gridy = 3;
            this.add(new JLabel("Confirmation message:"), c);

            confirmationMessage.setText("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
            confirmationMessage.addActionListener(this);
            c = new GridBagConstraints();
            c.gridx = 1;
            c.gridy = 3;
            c.fill = GridBagConstraints.BOTH;
            c.gridwidth = 2;
            this.add(confirmationMessage, c);
        }

        public void actionPerformed(ActionEvent e) {
            FrameBufferBuffer.this.renderNativeBuffer();
        }

        public DeviceInfoDB.Device currentDevice() {
            return (DeviceInfoDB.Device) deviceSelector.getSelectedItem();
        }

        public String currentLocale() {
            return (String)localeSelector.getSelectedItem();
        }

        public boolean magnified() {
            return magnifiedCheckbox.isSelected();
        }

        public boolean inverted() {
            return invertedCheckbox.isSelected();
        }

        public String confirmationMessage() {
            return confirmationMessage.getText();
        }
    }

    private BufferedImage mImage;
    private DataBufferInt mBuffer;
    private MagnifiedView mMagnifiedView;
    private ConfigSelector mConfigSelector;
    private JFrame mFrame;

    public MagnifiedView getMagnifiedView() {
        if (mMagnifiedView == null) {
            mMagnifiedView = new MagnifiedView();
        }
        return mMagnifiedView;
    }

    public ConfigSelector getConfigSelector(){
        if (mConfigSelector == null){
            mConfigSelector = new ConfigSelector();
        }
        return mConfigSelector;
    }

    @Override
    public void mouseDragged(MouseEvent e) {

    }

    @Override
    public void mouseMoved(MouseEvent e) {
        if (mMagnifiedView != null) {
            final int rMask = 0xff;
            final int gMask = 0xff00;
            final int bMask = 0xff0000;
            final int bpp = 24;
            int d = mMagnifiedView.d();
            int x = e.getX() - d/2;
            if (x + d > mImage.getWidth()) {
                x = mImage.getWidth() - d;
            }
            if (x < 0) { x = 0; }
            int y = e.getY() - d/2;
            if (y + d > mImage.getHeight()) {
                y = mImage.getHeight() - d;
            }
            if (y < 0) { y = 0; }
            if (mImage.getWidth() < d) {
                d = mImage.getWidth();
            }
            if (mImage.getHeight() < d) {
                d = mImage.getHeight();
            }
            mMagnifiedView.draw(mImage.getSubimage(x, y, d, d));
        }
    }


    public FrameBufferBuffer() {
        setSize(412, 900);
        setPreferredSize(new Dimension(412, 900));
        renderNativeBuffer();
        addComponentListener(this);
        addMouseMotionListener(this);
    }

    @Override
    public void componentResized(ComponentEvent e) {
        renderNativeBuffer();
        repaint();
    }
    @Override
    public void componentMoved(ComponentEvent e) {

    }

    @Override
    public void componentShown(ComponentEvent e) {

    }

    @Override
    public void componentHidden(ComponentEvent e) {

    }

    @Override
    public void paint(Graphics g) {
        synchronized (this) {
            g.drawImage(mImage, 0, 0, null);
        }
    }

    public void setFrame(JFrame frame){
        mFrame = frame;
    }

    public void renderNativeBuffer(){
        DeviceInfo deviceInfo = DeviceInfoDB.getDeviceInfo(getConfigSelector().currentDevice());
        boolean magnified = getConfigSelector().magnified();
        boolean inverted = getConfigSelector().inverted();
        NativeRenderer.setLanguage(getConfigSelector().currentLocale());
        NativeRenderer.setConfimationMessage(getConfigSelector().confirmationMessage());
        int w = deviceInfo.getWidthPx();
        int h = deviceInfo.getHeightPx();
        final int linestride = w;
        final int rMask = 0xff;
        final int gMask = 0xff00;
        final int bMask = 0xff0000;
        final int bpp = 24;
        int error = 0;
        synchronized (this) {
            mBuffer = new DataBufferInt(h * linestride);
            WritableRaster raster = Raster.createPackedRaster(mBuffer, w, h, linestride,
                new int[]{rMask, gMask, bMask}, null);
            ColorModel colorModel = new DirectColorModel(bpp, rMask, gMask, bMask);
            BufferedImage image = new BufferedImage(colorModel, raster, true, null);
            NativeRenderer.setDeviceInfo(deviceInfo, magnified, inverted);
            error = NativeRenderer.renderBuffer(0, 0, w, h, linestride, mBuffer.getData());
            if (error != 0) {
                System.out.println("Error rendering native buffer " + error);
            }

            mImage = new BufferedImage(getWidth(), getHeight(), BufferedImage.TYPE_INT_ARGB_PRE);
            Graphics2D gc = mImage.createGraphics();
            double scale = 0.0;
            if (w/(double)h > getWidth()/(double)getHeight()) {
                scale = (double)getWidth()/(double)w;
            } else {
                scale = (double)getHeight()/(double)h;
            }
            gc.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            gc.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
            gc.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
            gc.drawRenderedImage(image, AffineTransform.getScaleInstance(scale, scale));
        }
        repaint();
    }
}
