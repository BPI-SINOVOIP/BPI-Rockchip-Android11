/*
 * Copyright 2018 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Notes
 *
 * TODO (chriswailes): Build a unified error reporting mechanism.
 * TODO (chriswailes): Support CSV output
 */

/*
 * Imports
 */

import java.io.File

import kotlin.math.pow
import kotlin.math.sqrt
import kotlin.system.exitProcess

import trebuchet.extras.parseTrace
import trebuchet.model.SchedulingState
import trebuchet.util.slices.*
import trebuchet.util.time.*

/*
 * Constants
 */

val FILENAME_PATTERN = Regex("launch-(\\w+)-(\\w+)-(quicken|speed|speed-profile)-(cold|warm)-\\d")

val INTERESTING_SLICES = arrayOf(
    SLICE_NAME_ACTIVITY_START,
    SLICE_NAME_ACTIVITY_THREAD_MAIN,
    SLICE_NAME_APP_IMAGE_INTERN_STRING,
    SLICE_NAME_APP_IMAGE_LOADING,
    SLICE_NAME_BIND_APPLICATION,
    SLICE_NAME_INFLATE,
    SLICE_NAME_POST_FORK,
    SLICE_NAME_ZYGOTE_INIT)

const val SAMPLE_THRESHOLD_APPLICATION = 5
const val SAMPLE_THRESHOLD_COMPILER = 5
const val SAMPLE_THRESHOLD_TEMPERATURE = 5

/*
 * Class Definition
 */

enum class CompilerFilter {
    QUICKEN,
    SPEED,
    SPEED_PROFILE
}

enum class Temperature {
    COLD,
    WARM
}

class CompilerRecord(val cold : MutableList<StartupEvent> = mutableListOf(),
                     val warm : MutableList<StartupEvent> = mutableListOf()) {

    fun numSamples() : Int {
        return this.cold.size + this.warm.size
    }
}

class ApplicationRecord(val quicken : CompilerRecord = CompilerRecord(),
                        val speed : CompilerRecord = CompilerRecord(),
                        val speedProfile : CompilerRecord = CompilerRecord()) {

    fun numSamples() : Int {
        return this.quicken.numSamples() + this.speed.numSamples() + this.speedProfile.numSamples()
    }
}

/*
 * Class Extensions
 */

/*
 * Helper Functions
 */

fun addStartupRecord(records : MutableMap<String, ApplicationRecord>, startupEvent : StartupEvent, appName : String, compiler : CompilerFilter, temperature : Temperature) {
    val applicationRecord = records.getOrPut(appName) { ApplicationRecord() }

    val compilerRecord =
        when (compiler) {
            CompilerFilter.QUICKEN -> applicationRecord.quicken
            CompilerFilter.SPEED -> applicationRecord.speed
            CompilerFilter.SPEED_PROFILE -> applicationRecord.speedProfile
        }

    when (temperature) {
        Temperature.COLD -> compilerRecord.cold
        Temperature.WARM -> compilerRecord.warm
    }.add(startupEvent)
}

fun averageAndStandardDeviation(values : List<Double>) : Pair<Double, Double> {
    val total = values.sum()
    val averageValue = total / values.size

    var sumOfSquaredDiffs = 0.0

    values.forEach { value ->
        sumOfSquaredDiffs += (value - averageValue).pow(2)
    }

    return Pair(averageValue, sqrt(sumOfSquaredDiffs / values.size))
}

fun parseFileName(fileName : String) : Triple<String, CompilerFilter, Temperature> {
    val md = FILENAME_PATTERN.find(fileName)

    if (md != null) {
        val compilerFilter =
            when (md.groups[3]!!.value) {
                "quicken" -> CompilerFilter.QUICKEN
                "speed" -> CompilerFilter.SPEED
                "speed-profile" -> CompilerFilter.SPEED_PROFILE
                else -> throw Exception("Bad compiler match data.")
            }

        val temperature =
            when (md.groups[4]!!.value) {
                "cold" -> Temperature.COLD
                "warm" -> Temperature.WARM
                else -> throw Exception("Bad temperature match data.")
            }

        return Triple(md.groups[1]!!.value, compilerFilter, temperature)
    } else {
        println("Unrecognized file name: $fileName")
        exitProcess(1)
    }
}

fun printPlainText(records : MutableMap<String, ApplicationRecord>) {
    records.forEach { appName, record ->
        if (record.numSamples() > SAMPLE_THRESHOLD_APPLICATION) {
            println("$appName:")
            printAppRecordPlainText(record)
            println()
        }
    }
}

fun printAppRecordPlainText(record : ApplicationRecord) {
    if (record.quicken.numSamples() > SAMPLE_THRESHOLD_COMPILER) {
        println("\tQuicken:")
        printCompilerRecordPlainText(record.quicken)
    }

    if (record.speed.numSamples() > SAMPLE_THRESHOLD_COMPILER) {
        println("\tSpeed:")
        printCompilerRecordPlainText(record.speed)
    }

    if (record.speedProfile.numSamples() > SAMPLE_THRESHOLD_COMPILER) {
        println("\tSpeed Profile:")
        printCompilerRecordPlainText(record.speedProfile)
    }
}

fun printCompilerRecordPlainText(record : CompilerRecord) {
    if (record.cold.size > SAMPLE_THRESHOLD_TEMPERATURE) {
        println("\t\tCold:")
        printSampleSetPlainText(record.cold)
    }

    if (record.warm.size > SAMPLE_THRESHOLD_TEMPERATURE) {
        println("\t\tWarm:")
        printSampleSetPlainText(record.warm)
    }
}

fun printSampleSetPlainText(records : List<StartupEvent>) {

    val (startupTimeAverage, startupTimeStandardDeviation) = averageAndStandardDeviation(records.map {it.endTime - it.startTime})
    val (timeToFirstSliceAverage, timeToFirstSliceStandardDeviation) = averageAndStandardDeviation(records.map {it.firstSliceTime - it.startTime})
    val (undifferentiatedTimeAverage, undifferentiatedTimeStandardDeviation) = averageAndStandardDeviation(records.map {it.undifferentiatedTime})

    println("\t\t\tAverage startup time:            ${startupTimeAverage.secondValueToMillisecondString()}")
    println("\t\t\tStartup time standard deviation: ${startupTimeStandardDeviation.secondValueToMillisecondString()}")
    println("\t\t\tAverage time to first slice:     ${timeToFirstSliceAverage.secondValueToMillisecondString()}")
    println("\t\t\tTTFS standard deviation:         ${timeToFirstSliceStandardDeviation.secondValueToMillisecondString()}")
    println("\t\t\tAverage undifferentiated time:   ${undifferentiatedTimeAverage.secondValueToMillisecondString()}")
    println("\t\t\tUDT standard deviation:          ${undifferentiatedTimeStandardDeviation.secondValueToMillisecondString()}")

    if (records.first().reportFullyDrawnTime != null) {
        val (rfdTimeAverage, rfdTimeStandardDeviation) = averageAndStandardDeviation(records.map { it.reportFullyDrawnTime!! - it.startTime})
        println("\t\t\tAverage RFD time:                ${rfdTimeAverage.secondValueToMillisecondString()}")
        println("\t\t\tRFD time standard deviation:     ${rfdTimeStandardDeviation.secondValueToMillisecondString()}")
    }

    println()

    println("\t\t\tScheduling info:")
    SchedulingState.values().toSortedSet().forEach { schedState ->
        val (schedStateTimeAverage, schedStateTimeStandardDeviation) = averageAndStandardDeviation(records.map {it.schedTimings.getOrDefault(schedState, 0.0)})

        if (schedStateTimeAverage != 0.0) {
            println(
                "\t\t\t\t${schedState.friendlyName}: ${schedStateTimeAverage.secondValueToMillisecondString()} / ${schedStateTimeStandardDeviation.secondValueToMillisecondString()}")
        }
    }

    println()

    // Definition of printing helper.
    fun printSliceTimings(sliceInfos : List<AggregateSliceInfoMap>, filterSlices : Boolean) {
        val samples : MutableMap<String, MutableList<Double>> = mutableMapOf()

        sliceInfos.forEach { sliceInfo ->
            sliceInfo.forEach {sliceName, aggInfo ->
                samples.getOrPut(sliceName, ::mutableListOf).add(aggInfo.totalTime)
            }
        }

        samples.forEach {sliceName, sliceSamples ->
            if (!filterSlices || INTERESTING_SLICES.contains(sliceName)) {
                val (sliceDurationAverage, sliceDurationStandardDeviation) = averageAndStandardDeviation(
                    sliceSamples)

                println("\t\t\t\t$sliceName:")

                println(
                    "\t\t\t\t\tAverage duration:     ${sliceDurationAverage.secondValueToMillisecondString()}")
                println(
                    "\t\t\t\t\tStandard deviation:   ${sliceDurationStandardDeviation.secondValueToMillisecondString()}")
                println("\t\t\t\t\tStartup time percent: %.2f%%".format(
                    (sliceDurationAverage / startupTimeAverage) * 100))
            }
        }
    }

    /*
     * Timing accumulation
     */

    println("\t\t\tTop-level timings:")
    printSliceTimings(records.map {it.topLevelSliceInfo}, false)
    println()
    println("\t\t\tNon-nested timings:")
    printSliceTimings(records.map {it.nonNestedSliceInfo}, true)
    println()
    println("\t\t\tUndifferentiated timing:")
    printSliceTimings(records.map {it.undifferentiatedSliceInfo}, true)
    println()
}

fun printCSV(records : MutableMap<String, ApplicationRecord>) {
    println("Application Name, Compiler Filter, Temperature, Startup Time Avg (ms), Startup Time SD (ms), Time-to-first-slice Avg (ms), Time-to-first-slice SD (ms), Report Fully Drawn Avg (ms), Report Fully Drawn SD (ms)")

    records.forEach { appName, record ->
        if (record.numSamples() > SAMPLE_THRESHOLD_APPLICATION) {
            printAppRecordCSV(appName, record)
        }
    }
}

fun printAppRecordCSV(appName : String, record : ApplicationRecord) {
    if (record.quicken.numSamples() > SAMPLE_THRESHOLD_COMPILER) {
        printCompilerRecordCSV(appName, "quicken", record.quicken)
    }

    if (record.speed.numSamples() > SAMPLE_THRESHOLD_COMPILER) {
        printCompilerRecordCSV(appName, "speed", record.speed)
    }

    if (record.speedProfile.numSamples() > SAMPLE_THRESHOLD_COMPILER) {
        printCompilerRecordCSV(appName, "speed-profile", record.speedProfile)
    }
}

fun printCompilerRecordCSV(appName : String, compilerFilter : String, record : CompilerRecord) {
    if (record.cold.size > SAMPLE_THRESHOLD_TEMPERATURE) {
        printSampleSetCSV(appName, compilerFilter, "cold", record.cold)
    }

    if (record.warm.size > SAMPLE_THRESHOLD_TEMPERATURE) {
        printSampleSetCSV(appName, compilerFilter, "warm", record.warm)
    }
}

fun printSampleSetCSV(appName : String, compilerFilter : String, temperature : String, records : List<StartupEvent>) {
    print("$appName, $compilerFilter, $temperature, ")

    val (startupTimeAverage, startupTimeStandardDeviation) = averageAndStandardDeviation(records.map {it.endTime - it.startTime})
    print("%.3f, %.3f, ".format(startupTimeAverage * 1000, startupTimeStandardDeviation * 1000))

    val (timeToFirstSliceAverage, timeToFirstSliceStandardDeviation) = averageAndStandardDeviation(records.map {it.firstSliceTime - it.startTime})
    print("%.3f, %.3f,".format(timeToFirstSliceAverage * 1000, timeToFirstSliceStandardDeviation * 1000))

    if (records.first().reportFullyDrawnTime != null) {
        val (rfdTimeAverage, rfdTimeStandardDeviation) = averageAndStandardDeviation(records.map { it.reportFullyDrawnTime!! - it.startTime})
        print(" %.3f, %.3f\n".format(rfdTimeAverage * 1000, rfdTimeStandardDeviation * 1000))

    } else {
        print(",\n")
    }
}

/*
 * Main Function
 */

fun main(args: Array<String>) {
    if (args.isEmpty()) {
        println("Usage: StartupSummarizer <trace files>")
        return
    }

    val records : MutableMap<String, ApplicationRecord> = mutableMapOf()
    val exceptions : MutableList<Pair<String, Exception>> = mutableListOf()

    var processedFiles = 0
    var erroredFiles = 0

    args.forEach { fileName ->
        val (_, compiler, temperature) = parseFileName(fileName)

        val trace = parseTrace(File(fileName), false)

        try {
            val traceStartupEvents : List<StartupEvent> = trace.getStartupEvents()

            traceStartupEvents.forEach { startupEvent ->
                addStartupRecord(records, startupEvent, startupEvent.name, compiler,
                                 temperature)
            }

        } catch (e: Exception) {
            exceptions.add(Pair(fileName, e))
            ++erroredFiles
        } finally {
            ++processedFiles

            print("\rProgress: $processedFiles ($erroredFiles) / ${args.size}")
        }
    }

    println()

//    printPlainText(records)
    printCSV(records)

    if (exceptions.count() > 0) {
        println("Exceptions:")
        exceptions.forEach { (fileName, exception) ->
            println("\t$fileName: $exception${if (exception.message != null) "(${exception.message})" else ""}")
        }
    }
}