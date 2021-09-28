package android.net.testutils

import android.net.LinkAddress
import android.net.LinkProperties
import android.net.Network
import android.net.NetworkCapabilities
import com.android.testutils.ConcurrentIntepreter
import com.android.testutils.InterpretMatcher
import com.android.testutils.RecorderCallback.CallbackEntry
import com.android.testutils.RecorderCallback.CallbackEntry.Available
import com.android.testutils.RecorderCallback.CallbackEntry.BlockedStatus
import com.android.testutils.RecorderCallback.CallbackEntry.CapabilitiesChanged
import com.android.testutils.TestableNetworkCallback
import com.android.testutils.intArg
import com.android.testutils.strArg
import com.android.testutils.timeArg
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import kotlin.reflect.KClass
import kotlin.test.assertEquals
import kotlin.test.assertFails
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.test.fail

const val SHORT_TIMEOUT_MS = 20L
const val DEFAULT_LINGER_DELAY_MS = 30000
const val NOT_METERED = NetworkCapabilities.NET_CAPABILITY_NOT_METERED
const val WIFI = NetworkCapabilities.TRANSPORT_WIFI
const val CELLULAR = NetworkCapabilities.TRANSPORT_CELLULAR
const val TEST_INTERFACE_NAME = "testInterfaceName"

@RunWith(JUnit4::class)
class TestableNetworkCallbackTest {
    private lateinit var mCallback: TestableNetworkCallback

    private fun makeHasNetwork(netId: Int) = object : TestableNetworkCallback.HasNetwork {
        override val network: Network = Network(netId)
    }

    @Before
    fun setUp() {
        mCallback = TestableNetworkCallback()
    }

    @Test
    fun testLastAvailableNetwork() {
        // Make sure there is no last available network at first, then the last available network
        // is returned after onAvailable is called.
        val net2097 = Network(2097)
        assertNull(mCallback.lastAvailableNetwork)
        mCallback.onAvailable(net2097)
        assertEquals(mCallback.lastAvailableNetwork, net2097)

        // Make sure calling onCapsChanged/onLinkPropertiesChanged don't affect the last available
        // network.
        mCallback.onCapabilitiesChanged(net2097, NetworkCapabilities())
        mCallback.onLinkPropertiesChanged(net2097, LinkProperties())
        assertEquals(mCallback.lastAvailableNetwork, net2097)

        // Make sure onLost clears the last available network.
        mCallback.onLost(net2097)
        assertNull(mCallback.lastAvailableNetwork)

        // Do the same but with a different network after onLost : make sure the last available
        // network is the new one, not the original one.
        val net2098 = Network(2098)
        mCallback.onAvailable(net2098)
        mCallback.onCapabilitiesChanged(net2098, NetworkCapabilities())
        mCallback.onLinkPropertiesChanged(net2098, LinkProperties())
        assertEquals(mCallback.lastAvailableNetwork, net2098)

        // Make sure onAvailable changes the last available network even if onLost was not called.
        val net2099 = Network(2099)
        mCallback.onAvailable(net2099)
        assertEquals(mCallback.lastAvailableNetwork, net2099)

        // For legacy reasons, lastAvailableNetwork is null as soon as any is lost, not necessarily
        // the last available one. Check that behavior.
        mCallback.onLost(net2098)
        assertNull(mCallback.lastAvailableNetwork)

        // Make sure that losing the really last available one still results in null.
        mCallback.onLost(net2099)
        assertNull(mCallback.lastAvailableNetwork)

        // Make sure multiple onAvailable in a row then onLost still results in null.
        mCallback.onAvailable(net2097)
        mCallback.onAvailable(net2098)
        mCallback.onAvailable(net2099)
        mCallback.onLost(net2097)
        assertNull(mCallback.lastAvailableNetwork)
    }

    @Test
    fun testAssertNoCallback() {
        mCallback.assertNoCallback(SHORT_TIMEOUT_MS)
        mCallback.onAvailable(Network(100))
        assertFails { mCallback.assertNoCallback(SHORT_TIMEOUT_MS) }
    }

    @Test
    fun testCapabilitiesWithAndWithout() {
        val net = Network(101)
        val matcher = makeHasNetwork(101)
        val meteredNc = NetworkCapabilities()
        val unmeteredNc = NetworkCapabilities().addCapability(NOT_METERED)
        // Check that expecting caps (with or without) fails when no callback has been received.
        assertFails { mCallback.expectCapabilitiesWith(NOT_METERED, matcher, SHORT_TIMEOUT_MS) }
        assertFails { mCallback.expectCapabilitiesWithout(NOT_METERED, matcher, SHORT_TIMEOUT_MS) }

        // Add NOT_METERED and check that With succeeds and Without fails.
        mCallback.onCapabilitiesChanged(net, unmeteredNc)
        mCallback.expectCapabilitiesWith(NOT_METERED, matcher)
        mCallback.onCapabilitiesChanged(net, unmeteredNc)
        assertFails { mCallback.expectCapabilitiesWithout(NOT_METERED, matcher, SHORT_TIMEOUT_MS) }

        // Don't add NOT_METERED and check that With fails and Without succeeds.
        mCallback.onCapabilitiesChanged(net, meteredNc)
        assertFails { mCallback.expectCapabilitiesWith(NOT_METERED, matcher, SHORT_TIMEOUT_MS) }
        mCallback.onCapabilitiesChanged(net, meteredNc)
        mCallback.expectCapabilitiesWithout(NOT_METERED, matcher)
    }

    @Test
    fun testExpectCallbackThat() {
        val net = Network(193)
        val netCaps = NetworkCapabilities().addTransportType(CELLULAR)
        // Check that expecting callbackThat anything fails when no callback has been received.
        assertFails { mCallback.expectCallbackThat(SHORT_TIMEOUT_MS) { true } }

        // Basic test for true and false
        mCallback.onAvailable(net)
        mCallback.expectCallbackThat { true }
        mCallback.onAvailable(net)
        assertFails { mCallback.expectCallbackThat(SHORT_TIMEOUT_MS) { false } }

        // Try a positive and a negative case
        mCallback.onBlockedStatusChanged(net, true)
        mCallback.expectCallbackThat { cb -> cb is BlockedStatus && cb.blocked }
        mCallback.onCapabilitiesChanged(net, netCaps)
        assertFails { mCallback.expectCallbackThat(SHORT_TIMEOUT_MS) { cb ->
            cb is CapabilitiesChanged && cb.caps.hasTransport(WIFI)
        } }
    }

    @Test
    fun testCapabilitiesThat() {
        val net = Network(101)
        val netCaps = NetworkCapabilities().addCapability(NOT_METERED).addTransportType(WIFI)
        // Check that expecting capabilitiesThat anything fails when no callback has been received.
        assertFails { mCallback.expectCapabilitiesThat(net, SHORT_TIMEOUT_MS) { true } }

        // Basic test for true and false
        mCallback.onCapabilitiesChanged(net, netCaps)
        mCallback.expectCapabilitiesThat(net) { true }
        mCallback.onCapabilitiesChanged(net, netCaps)
        assertFails { mCallback.expectCapabilitiesThat(net, SHORT_TIMEOUT_MS) { false } }

        // Try a positive and a negative case
        mCallback.onCapabilitiesChanged(net, netCaps)
        mCallback.expectCapabilitiesThat(net) { caps ->
            caps.hasCapability(NOT_METERED) &&
                    caps.hasTransport(WIFI) &&
                    !caps.hasTransport(CELLULAR)
        }
        mCallback.onCapabilitiesChanged(net, netCaps)
        assertFails { mCallback.expectCapabilitiesThat(net, SHORT_TIMEOUT_MS) { caps ->
            caps.hasTransport(CELLULAR)
        } }

        // Try a matching callback on the wrong network
        mCallback.onCapabilitiesChanged(net, netCaps)
        assertFails { mCallback.expectCapabilitiesThat(Network(100), SHORT_TIMEOUT_MS) { true } }
    }

    @Test
    fun testLinkPropertiesThat() {
        val net = Network(112)
        val linkAddress = LinkAddress("fe80::ace:d00d/64")
        val mtu = 1984
        val linkProps = LinkProperties().apply {
            this.mtu = mtu
            interfaceName = TEST_INTERFACE_NAME
            addLinkAddress(linkAddress)
        }

        // Check that expecting linkPropsThat anything fails when no callback has been received.
        assertFails { mCallback.expectLinkPropertiesThat(net, SHORT_TIMEOUT_MS) { true } }

        // Basic test for true and false
        mCallback.onLinkPropertiesChanged(net, linkProps)
        mCallback.expectLinkPropertiesThat(net) { true }
        mCallback.onLinkPropertiesChanged(net, linkProps)
        assertFails { mCallback.expectLinkPropertiesThat(net, SHORT_TIMEOUT_MS) { false } }

        // Try a positive and negative case
        mCallback.onLinkPropertiesChanged(net, linkProps)
        mCallback.expectLinkPropertiesThat(net) { lp ->
            lp.interfaceName == TEST_INTERFACE_NAME &&
                    lp.linkAddresses.contains(linkAddress) &&
                    lp.mtu == mtu
        }
        mCallback.onLinkPropertiesChanged(net, linkProps)
        assertFails { mCallback.expectLinkPropertiesThat(net, SHORT_TIMEOUT_MS) { lp ->
            lp.interfaceName != TEST_INTERFACE_NAME
        } }

        // Try a matching callback on the wrong network
        mCallback.onLinkPropertiesChanged(net, linkProps)
        assertFails { mCallback.expectLinkPropertiesThat(Network(114), SHORT_TIMEOUT_MS) { lp ->
            lp.interfaceName == TEST_INTERFACE_NAME
        } }
    }

    @Test
    fun testExpectCallback() {
        val net = Network(103)
        // Test expectCallback fails when nothing was sent.
        assertFails { mCallback.expectCallback<BlockedStatus>(net, SHORT_TIMEOUT_MS) }

        // Test onAvailable is seen and can be expected
        mCallback.onAvailable(net)
        mCallback.expectCallback<Available>(net, SHORT_TIMEOUT_MS)

        // Test onAvailable won't return calls with a different network
        mCallback.onAvailable(Network(106))
        assertFails { mCallback.expectCallback<Available>(net, SHORT_TIMEOUT_MS) }

        // Test onAvailable won't return calls with a different callback
        mCallback.onAvailable(net)
        assertFails { mCallback.expectCallback<BlockedStatus>(net, SHORT_TIMEOUT_MS) }
    }

    @Test
    fun testPollForNextCallback() {
        assertFails { mCallback.pollForNextCallback(SHORT_TIMEOUT_MS) }
        TNCInterpreter.interpretTestSpec(initial = mCallback, lineShift = 1,
                threadTransform = { cb -> cb.createLinkedCopy() }, spec = """
            sleep; onAvailable(133)    | poll(2) = Available(133) time 1..4
                                       | poll(1) fails
            onCapabilitiesChanged(108) | poll(1) = CapabilitiesChanged(108) time 0..3
            onBlockedStatus(199)       | poll(1) = BlockedStatus(199) time 0..3
        """)
    }
}

private object TNCInterpreter : ConcurrentIntepreter<TestableNetworkCallback>(interpretTable)

val EntryList = CallbackEntry::class.sealedSubclasses.map { it.simpleName }.joinToString("|")
private fun callbackEntryFromString(name: String): KClass<out CallbackEntry> {
    return CallbackEntry::class.sealedSubclasses.first { it.simpleName == name }
}

private val interpretTable = listOf<InterpretMatcher<TestableNetworkCallback>>(
    // Interpret "Available(xx)" as "call to onAvailable with netId xx", and likewise for
    // all callback types. This is implemented above by enumerating the subclasses of
    // CallbackEntry and reading their simpleName.
    Regex("""(.*)\s+=\s+($EntryList)\((\d+)\)""") to { i, cb, t ->
        val record = i.interpret(t.strArg(1), cb)
        assertTrue(callbackEntryFromString(t.strArg(2)).isInstance(record))
        // Strictly speaking testing for is CallbackEntry is useless as it's been tested above
        // but the compiler can't figure things out from the isInstance call. It does understand
        // from the assertTrue(is CallbackEntry) that this is true, which allows to access
        // the 'network' member below.
        assertTrue(record is CallbackEntry)
        assertEquals(record.network.netId, t.intArg(3))
    },
    // Interpret "onAvailable(xx)" as calling "onAvailable" with a netId of xx, and likewise for
    // all callback types. NetworkCapabilities and LinkProperties just get an empty object
    // as their argument. Losing gets the default linger timer. Blocked gets false.
    Regex("""on($EntryList)\((\d+)\)""") to { i, cb, t ->
        val net = Network(t.intArg(2))
        when (t.strArg(1)) {
            "Available" -> cb.onAvailable(net)
            // PreCheck not used in tests. Add it here if it becomes useful.
            "CapabilitiesChanged" -> cb.onCapabilitiesChanged(net, NetworkCapabilities())
            "LinkPropertiesChanged" -> cb.onLinkPropertiesChanged(net, LinkProperties())
            "Suspended" -> cb.onNetworkSuspended(net)
            "Resumed" -> cb.onNetworkResumed(net)
            "Losing" -> cb.onLosing(net, DEFAULT_LINGER_DELAY_MS)
            "Lost" -> cb.onLost(net)
            "Unavailable" -> cb.onUnavailable()
            "BlockedStatus" -> cb.onBlockedStatusChanged(net, false)
            else -> fail("Unknown callback type")
        }
    },
    Regex("""poll\((\d+)\)""") to { i, cb, t ->
        cb.pollForNextCallback(t.timeArg(1))
    }
)
