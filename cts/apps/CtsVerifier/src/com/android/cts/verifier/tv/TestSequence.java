package com.android.cts.verifier.tv;


import com.android.cts.verifier.tv.TvAppVerifierActivity;

import java.util.List;
import java.util.stream.Collectors;

/**
 * A sequence of {@link TestStepBase}s within a {@link TvAppVerifierActivity}, which are meant to be
 * executed one after another. The whole sequence passes if all containing test steps pass.
 */
public class TestSequence {
    private List<TestStepBase> steps;
    private TvAppVerifierActivity context;

    /**
     * @param context The TvAppVerifierActivity containing this sequence.
     * @param steps List of the steps contained in the sequence.
     */
    public TestSequence(TvAppVerifierActivity context, List<TestStepBase> steps) {
        this.context = context;
        this.steps = steps;
    }

    /**
     * Initializes the steps in the sequence by creating their UI components, ensuring they can be
     * executed only in the given order and properly enable the pass/fails buttons of the
     * surrounding {@link TvAppVerifierActivity}. This method should be called in the
     * createTestItems() method of the {@link TvAppVerifierActivity} which contains this sequence.
     */
    public void init() {
        if (steps.isEmpty()) {
            return;
        }

        // Initialize all containing test steps.
        steps.stream().forEach(step -> step.createUiElements());

        // After a step is completed we enable the button of the next step.
        for (int i = 0; i < steps.size() - 1; i++) {
            final int next = i + 1;
            steps.get(i).setOnDoneListener(() -> steps.get(next).enableInteractivity());
        }

        // When the last step is done, mark the sequence as done.
        steps.get(steps.size() - 1)
                .setOnDoneListener(() -> onAllStepsDone());

        // Enable the button of the first test step so the user can start it.
        steps.get(0).enableInteractivity();
    }

    public String getFailureDetails() {
        return steps.stream()
                .filter(step -> !step.hasPassed())
                .map(step -> step.getFailureDetails())
                .collect(Collectors.joining("\n"));
    }

    private void onAllStepsDone() {
        // The sequence passes if all containing test steps pass.
        boolean allTestStepsPass = steps.stream().allMatch(step -> step.hasPassed());
        context.getPassButton().setEnabled(allTestStepsPass);
    }
}
