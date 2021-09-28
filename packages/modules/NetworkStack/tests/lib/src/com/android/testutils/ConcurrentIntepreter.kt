package com.android.testutils

import android.os.SystemClock
import java.util.concurrent.CyclicBarrier
import kotlin.system.measureTimeMillis
import kotlin.test.assertEquals
import kotlin.test.assertFails
import kotlin.test.assertNull
import kotlin.test.assertTrue

// The table contains pairs associating a regexp with the code to run. The statement is matched
// against each matcher in sequence and when a match is found the associated code is run, passing
// it the TrackRecord under test and the result of the regexp match.
typealias InterpretMatcher<T> = Pair<Regex, (ConcurrentIntepreter<T>, T, MatchResult) -> Any?>

// The default unit of time for interpreted tests
val INTERPRET_TIME_UNIT = 40L // ms

/**
 * A small interpreter for testing parallel code. The interpreter will read a list of lines
 * consisting of "|"-separated statements. Each column runs in a different concurrent thread
 * and all threads wait for each other in between lines. Each statement is split on ";" then
 * matched with regular expressions in the instructionTable constant, which contains the
 * code associated with each statement. The interpreter supports an object being passed to
 * the interpretTestSpec() method to be passed in each lambda (think about the object under
 * test), and an optional transform function to be executed on the object at the start of
 * every thread.
 *
 * The time unit is defined in milliseconds by the interpretTimeUnit member, which has a default
 * value but can be passed to the constructor. Whitespace is ignored.
 *
 * The interpretation table has to be passed as an argument. It's a table associating a regexp
 * with the code that should execute, as a function taking three arguments : the interpreter,
 * the regexp match, and the object. See the individual tests for the DSL of that test.
 * Implementors for new interpreting languages are encouraged to look at the defaultInterpretTable
 * constant below for an example of how to write an interpreting table.
 * Some expressions already exist by default and can be used by all interpreters. They include :
 * sleep(x) : sleeps for x time units and returns Unit ; sleep alone means sleep(1)
 * EXPR = VALUE : asserts that EXPR equals VALUE. EXPR is interpreted. VALUE can either be the
 *   string "null" or an int. Returns Unit.
 * EXPR time x..y : measures the time taken by EXPR and asserts it took at least x and at most
 *   y time units.
 * EXPR // any text : comments are ignored.
 */
open class ConcurrentIntepreter<T>(
    localInterpretTable: List<InterpretMatcher<T>>,
    val interpretTimeUnit: Long = INTERPRET_TIME_UNIT
) {
    private val interpretTable: List<InterpretMatcher<T>> =
            localInterpretTable + getDefaultInstructions()

    // Split the line into multiple statements separated by ";" and execute them. Return whatever
    // the last statement returned.
    fun interpretMultiple(instr: String, r: T): Any? {
        return instr.split(";").map { interpret(it.trim(), r) }.last()
    }

    // Match the statement to a regex and interpret it.
    fun interpret(instr: String, r: T): Any? {
        val (matcher, code) =
                interpretTable.find { instr matches it.first } ?: throw SyntaxException(instr)
        val match = matcher.matchEntire(instr) ?: throw SyntaxException(instr)
        return code(this, r, match)
    }

    // Spins as many threads as needed by the test spec and interpret each program concurrently,
    // having all threads waiting on a CyclicBarrier after each line.
    // |lineShift| says how many lines after the call the spec starts. This is used for error
    // reporting. Unfortunately AFAICT there is no way to get the line of an argument rather
    // than the line at which the expression starts.
    fun interpretTestSpec(
        spec: String,
        initial: T,
        lineShift: Int = 0,
        threadTransform: (T) -> T = { it }
    ) {
        // For nice stack traces
        val callSite = getCallingMethod()
        val lines = spec.trim().trim('\n').split("\n").map { it.split("|") }
        // |threads| contains arrays of strings that make up the statements of a thread : in other
        // words, it's an array that contains a list of statements for each column in the spec.
        val threadCount = lines[0].size
        assertTrue(lines.all { it.size == threadCount })
        val threadInstructions = (0 until threadCount).map { i -> lines.map { it[i].trim() } }
        val barrier = CyclicBarrier(threadCount)
        var crash: InterpretException? = null
        threadInstructions.mapIndexed { threadIndex, instructions ->
            Thread {
                val threadLocal = threadTransform(initial)
                barrier.await()
                var lineNum = 0
                instructions.forEach {
                    if (null != crash) return@Thread
                    lineNum += 1
                    try {
                        interpretMultiple(it, threadLocal)
                    } catch (e: Throwable) {
                        // If fail() or some exception was called, the thread will come here ; if
                        // the exception isn't caught the process will crash, which is not nice for
                        // testing. Instead, catch the exception, cancel other threads, and report
                        // nicely. Catch throwable because fail() is AssertionError, which inherits
                        // from Error.
                        crash = InterpretException(threadIndex, it,
                                callSite.lineNumber + lineNum + lineShift,
                                callSite.className, callSite.methodName, callSite.fileName, e)
                    }
                    barrier.await()
                }
            }.also { it.start() }
        }.forEach { it.join() }
        // If the test failed, crash with line number
        crash?.let { throw it }
    }

    // Helper to get the stack trace for a calling method
    private fun getCallingStackTrace(): Array<StackTraceElement> {
        try {
            throw RuntimeException()
        } catch (e: RuntimeException) {
            return e.stackTrace
        }
    }

    // Find the calling method. This is the first method in the stack trace that is annotated
    // with @Test.
    fun getCallingMethod(): StackTraceElement {
        val stackTrace = getCallingStackTrace()
        return stackTrace.find { element ->
            val clazz = Class.forName(element.className)
            // Because the stack trace doesn't list the formal arguments, find all methods with
            // this name and return this name if any of them is annotated with @Test.
            clazz.declaredMethods
                    .filter { method -> method.name == element.methodName }
                    .any { method -> method.getAnnotation(org.junit.Test::class.java) != null }
        } ?: stackTrace[3]
        // If no method is annotated return the 4th one, because that's what it usually is :
        // 0 is getCallingStackTrace, 1 is this method, 2 is ConcurrentInterpreter#interpretTestSpec
    }
}

private fun <T> getDefaultInstructions() = listOf<InterpretMatcher<T>>(
    // Interpret an empty line as doing nothing.
    Regex("") to { _, _, _ -> null },
    // Ignore comments.
    Regex("(.*)//.*") to { i, t, r -> i.interpret(r.strArg(1), t) },
    // Interpret "XXX time x..y" : run XXX and check it took at least x and not more than y
    Regex("""(.*)\s*time\s*(\d+)\.\.(\d+)""") to { i, t, r ->
        val time = measureTimeMillis { i.interpret(r.strArg(1), t) }
        assertTrue(time in r.timeArg(2)..r.timeArg(3), "$time not in ${r.timeArg(2)..r.timeArg(3)}")
    },
    // Interpret "XXX = YYY" : run XXX and assert its return value is equal to YYY. "null" supported
    Regex("""(.*)\s*=\s*(null|\d+)""") to { i, t, r ->
        i.interpret(r.strArg(1), t).also {
            if ("null" == r.strArg(2)) assertNull(it) else assertEquals(r.intArg(2), it)
        }
    },
    // Interpret sleep. Optional argument for the count, in INTERPRET_TIME_UNIT units.
    Regex("""sleep(\((\d+)\))?""") to { i, t, r ->
        SystemClock.sleep(if (r.strArg(2).isEmpty()) i.interpretTimeUnit else r.timeArg(2))
    },
    Regex("""(.*)\s*fails""") to { i, t, r ->
        assertFails { i.interpret(r.strArg(1), t) }
    }
)

class SyntaxException(msg: String, cause: Throwable? = null) : RuntimeException(msg, cause)
class InterpretException(
    threadIndex: Int,
    instr: String,
    lineNum: Int,
    className: String,
    methodName: String,
    fileName: String,
    cause: Throwable
) : RuntimeException("Failure: $instr", cause) {
    init {
        stackTrace = arrayOf(StackTraceElement(
                className,
                "$methodName:thread$threadIndex",
                fileName,
                lineNum)) + super.getStackTrace()
    }
}

// Some small helpers to avoid to say the large ".groupValues[index].trim()" every time
fun MatchResult.strArg(index: Int) = this.groupValues[index].trim()
fun MatchResult.intArg(index: Int) = strArg(index).toInt()
fun MatchResult.timeArg(index: Int) = INTERPRET_TIME_UNIT * intArg(index)
