# Copyright 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os.path
import its.caps
import its.device
import its.image
import its.objects

NAME = os.path.basename(__file__).split('.')[0]
NUM_TEST_FRAMES = 20
FD_MODE_OFF = 0
FD_MODE_SIMPLE = 1
FD_MODE_FULL = 2
W, H = 640, 480


def main():
    """Test face detection.
    """

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        props = cam.override_with_hidden_physical_camera_props(props)
        its.caps.skip_unless(its.caps.face_detect(props))
        mono_camera = its.caps.mono_camera(props)
        fd_modes = props['android.statistics.info.availableFaceDetectModes']
        a = props['android.sensor.info.activeArraySize']
        aw, ah = a['right'] - a['left'], a['bottom'] - a['top']
        if its.caps.read_3a(props):
            gain, exp, _, _, focus = cam.do_3a(get_results=True,
                                               mono_camera=mono_camera)
            print 'iso = %d' % gain
            print 'exp = %.2fms' % (exp*1.0E-6)
            if focus == 0.0:
                print 'fd = infinity'
            else:
                print 'fd = %.2fcm' % (1.0E2/focus)
        for fd_mode in fd_modes:
            assert FD_MODE_OFF <= fd_mode <= FD_MODE_FULL
            req = its.objects.auto_capture_request()
            req['android.statistics.faceDetectMode'] = fd_mode
            fmt = {'format': 'yuv', 'width': W, 'height': H}
            caps = cam.do_capture([req]*NUM_TEST_FRAMES, fmt)
            for i, cap in enumerate(caps):
                md = cap['metadata']
                assert md['android.statistics.faceDetectMode'] == fd_mode
                faces = md['android.statistics.faces']

                # 0 faces should be returned for OFF mode
                if fd_mode == FD_MODE_OFF:
                    assert not faces
                    continue
                # Face detection could take several frames to warm up,
                # but it should detect at least one face in last frame
                if i == NUM_TEST_FRAMES - 1:
                    img = its.image.convert_capture_to_rgb_image(
                            cap, props=props)
                    img = its.image.rotate_img_per_argv(img)
                    img_name = '%s_fd_mode_%s.jpg' % (NAME, fd_mode)
                    its.image.write_image(img, img_name)
                    if not faces:
                        print 'Error: no face detected in mode', fd_mode
                        assert 0
                if not faces:
                    continue

                print 'Frame %d face metadata:' % i
                print '  Faces:', faces
                print ''

                face_scores = [face['score'] for face in faces]
                face_rectangles = [face['bounds'] for face in faces]
                for score in face_scores:
                    assert score >= 1 and score <= 100
                # Face bounds should be within active array
                for j, rect in enumerate(face_rectangles):
                    print 'Checking face rectangle %d...' % j
                    rect_t = rect['top']
                    rect_b = rect['bottom']
                    rect_l = rect['left']
                    rect_r = rect['right']
                    assert rect_t < rect_b
                    assert rect_l < rect_r
                    l_msg = 'l: %d outside of active W: 0,%d' % (rect_l, aw)
                    r_msg = 'r: %d outside of active W: 0,%d' % (rect_r, aw)
                    t_msg = 't: %d outside active H: 0,%d' % (rect_t, ah)
                    b_msg = 'b: %d outside active H: 0,%d' % (rect_b, ah)
                    # Assert same order as face landmarks below
                    assert 0 <= rect_l <= aw, l_msg
                    assert 0 <= rect_r <= aw, r_msg
                    assert 0 <= rect_t <= ah, t_msg
                    assert 0 <= rect_b <= ah, b_msg

                # Face landmarks are reported if and only if fd_mode is FULL
                # Face ID should be -1 for SIMPLE and unique for FULL
                if fd_mode == FD_MODE_SIMPLE:
                    for face in faces:
                        assert 'leftEye' not in face
                        assert 'rightEye' not in face
                        assert 'mouth' not in face
                        assert face['id'] == -1
                elif fd_mode == FD_MODE_FULL:
                    face_ids = [face['id'] for face in faces]
                    assert len(face_ids) == len(set(face_ids))
                    # Face landmarks should be within face bounds
                    for k, face in enumerate(faces):
                        print 'Checking landmarks in face %d...' % k
                        l_eye = face['leftEye']
                        r_eye = face['rightEye']
                        mouth = face['mouth']
                        l, r = face['bounds']['left'], face['bounds']['right']
                        t, b = face['bounds']['top'], face['bounds']['bottom']
                        l_eye_x, l_eye_y = l_eye['x'], l_eye['y']
                        r_eye_x, r_eye_y = r_eye['x'], r_eye['y']
                        mouth_x, mouth_y = mouth['x'], mouth['y']
                        lx_msg = 'l: %d, r: %d, x: %d' % (l, r, l_eye_x)
                        ly_msg = 't: %d, b: %d, y: %d' % (t, b, l_eye_y)
                        rx_msg = 'l: %d, r: %d, x: %d' % (l, r, r_eye_x)
                        ry_msg = 't: %d, b: %d, y: %d' % (t, b, r_eye_y)
                        mx_msg = 'l: %d, r: %d, x: %d' % (l, r, mouth_x)
                        my_msg = 't: %d, b: %d, y: %d' % (t, b, mouth_y)
                        assert l <= l_eye_x <= r, lx_msg
                        assert t <= l_eye_y <= b, ly_msg
                        assert l <= r_eye_x <= r, rx_msg
                        assert t <= r_eye_y <= b, ry_msg
                        assert l <= mouth_x <= r, mx_msg
                        assert t <= mouth_y <= b, my_msg

if __name__ == '__main__':
    main()
