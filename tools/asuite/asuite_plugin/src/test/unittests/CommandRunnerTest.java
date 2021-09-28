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
 * limitations under the License.
 */
package unittests;

import com.android.atest.commandAdapter.CommandRunner;
import com.android.atest.toolWindow.AtestToolWindow;
import com.intellij.execution.configurations.GeneralCommandLine;
import com.intellij.openapi.project.Project;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

@RunWith(MockitoJUnitRunner.class)
public class CommandRunnerTest {

    @Mock AtestToolWindow atestToolWindowMock;
    @Mock Project project;

    /** Tests CommandRunner by ls command. */
    @Test
    public void testCommandRunnerByLs() throws NoSuchFieldException, IllegalAccessException {
        ArrayList<String> command = new ArrayList<String>();
        command.add("ls");
        CommandRunner lsCommand = new CommandRunner(command, "/");
        Field field = lsCommand.getClass().getDeclaredField("mCommand");
        field.setAccessible(true);
        GeneralCommandLine commandLine = (GeneralCommandLine) field.get(lsCommand);
        Assert.assertSame(commandLine.getCharset(), StandardCharsets.UTF_8);
        Assert.assertEquals(commandLine.getCommandLineString(), "ls");
    }

    /** Tests CommandRunner by lunch target and test target. */
    @Test
    public void testCommandRunnerByTarget() throws NoSuchFieldException, IllegalAccessException {
        CommandRunner lsCommand = new CommandRunner("a", "b", "/", atestToolWindowMock, project);
        Field field = lsCommand.getClass().getDeclaredField("mCommand");
        field.setAccessible(true);
        GeneralCommandLine commandLine = (GeneralCommandLine) field.get(lsCommand);
        Assert.assertSame(commandLine.getCharset(), StandardCharsets.UTF_8);
        Assert.assertEquals(
                commandLine.getCommandLineString(),
                "/bin/bash -c \"export USER_FROM_TOOL=\\\"IntelliJ_Atest_plugin\\\" "
                        + "&& source build/envsetup.sh && lunch a && atest b\"");
    }
}
