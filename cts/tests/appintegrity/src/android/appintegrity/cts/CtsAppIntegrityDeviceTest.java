/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.appintegrity.cts;

import android.content.integrity.IntegrityFormula;
import android.content.integrity.Rule;
import android.content.integrity.RuleSet;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import java.util.Arrays;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test public APIs defined for AppIntegrityManager.
 */
@RunWith(AndroidJUnit4.class)
public class CtsAppIntegrityDeviceTest {

    /**
     * Test app integrity rule creation for app package name equals formula.
     */
    @Test
    public void applicationPackageNameEqualsFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Application.packageNameEquals("package");
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for app isPreInstalled formula.
     */
    @Test
    public void applicationIsPreInstalledFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Application.isPreInstalled();
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for app certificates contain formula.
     */
    @Test
    public void applicationCertificatesContainFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Application.certificatesContain("certificate");
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for app version code equals formula.
     */
    @Test
    public void applicationVersionCodeEqualToFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Application.versionCodeEquals(20L);
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for app version code greater than formula.
     */
    @Test
    public void applicationVersionCodeGreaterThanFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Application.versionCodeGreaterThan(20L);
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for app version code greater than equal to formula.
     */
    @Test
    public void applicationVersionCodeGreaterThanEqualToFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Application.versionCodeGreaterThanOrEqualTo(20L);
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for installer package name equals formula.
     */
    @Test
    public void installerPackageNameEqualsFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Installer.packageNameEquals("installer");
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for installer not allowed by manifest formula.
     */
    @Test
    public void installerNotAllowedByManifestFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Installer.notAllowedByManifest();
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for installer certificates contain formula.
     */
    @Test
    public void installerCertificatesContainFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.Installer.certificatesContain("certificate");
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for source stamp certificate hash equals formula.
     */
    @Test
    public void sourceStampHashEqualsFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.SourceStamp.stampCertificateHashEquals("hash");
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for source stamp is not trusted formula.
     */
    @Test
    public void sourceStampNotTrustedFormulaApiAvailable() throws Exception {
        IntegrityFormula formula = IntegrityFormula.SourceStamp.notTrusted();
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for all (AND) formula.
     */
    @Test
    public void allFormulaApiAvailable() throws Exception {
        IntegrityFormula formula1 = IntegrityFormula.Application.packageNameEquals("package");
        IntegrityFormula formula2 = IntegrityFormula.Application.isPreInstalled();
        IntegrityFormula formula = IntegrityFormula.all(formula1, formula2);
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for any (OR) formula.
     */
    @Test
    public void anyFormulaApiAvailable() throws Exception {
        IntegrityFormula formula1 = IntegrityFormula.Application.packageNameEquals("package");
        IntegrityFormula formula2 = IntegrityFormula.Application.isPreInstalled();
        IntegrityFormula formula = IntegrityFormula.any(formula1, formula2);
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for NOT formula.
     */
    @Test
    public void notFormulaApiAvailable() throws Exception {
        IntegrityFormula formula =
                IntegrityFormula.not(IntegrityFormula.Application.packageNameEquals("package"));
        Assert.assertNotNull(formula);
    }

    /**
     * Test app integrity rule creation for RuleSet methods.
     */
    @Test
    public void ruleSetApiMethodsAvailable() throws Exception {
        String version = "version";
        Rule rule = new Rule(IntegrityFormula.Application.packageNameEquals("package"), Rule.DENY);
        RuleSet ruleSet =
                new RuleSet.Builder()
                    .setVersion(version)
                    .addRules(Arrays.asList(rule))
                    .build();
        Assert.assertNotNull(ruleSet);
        Assert.assertEquals(ruleSet.getVersion(), version);
        Assert.assertEquals(ruleSet.getRules().size(), 1);
    }
}
