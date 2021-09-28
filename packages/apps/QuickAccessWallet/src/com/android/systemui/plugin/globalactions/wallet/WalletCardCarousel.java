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

package com.android.systemui.plugin.globalactions.wallet;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.HapticFeedbackConstants;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;

import androidx.cardview.widget.CardView;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.LinearSmoothScroller;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerViewAccessibilityDelegate;

import java.util.Collections;
import java.util.List;

/**
 * Card Carousel for displaying Quick Access Wallet cards.
 */
class WalletCardCarousel extends RecyclerView {

    // A negative card margin is required because card shrinkage pushes the cards too far apart
    private static final float CARD_MARGIN_RATIO = -.03f;
    private static final float CARD_SCREEN_WIDTH_RATIO = .69f;
    // Size of the unselected card as a ratio to size of selected card.
    private static final float UNSELECTED_CARD_SCALE = .83f;
    private static final float CORNER_RADIUS_RATIO = 25f / 700f;
    private static final float CARD_ASPECT_RATIO = 700f / 440f;
    static final int CARD_ANIM_ALPHA_DURATION = 100;
    static final int CARD_ANIM_ALPHA_DELAY = 50;

    private final int mScreenWidth;
    private final int mCardMarginPx;
    private final Rect mSystemGestureExclusionZone = new Rect();
    private final WalletCardAdapter mWalletCardAdapter;
    private final int mCardWidthPx;
    private final int mCardHeightPx;
    private final float mCornerRadiusPx;
    private final int mTotalCardWidth;
    private final float mCardEdgeToCenterDistance;

    private OnSelectionListener mSelectionListener;
    private OnCardScrollListener mCardScrollListener;
    // Adapter position of the child that is closest to the center of the recycler view.
    private int mCenteredAdapterPosition = RecyclerView.NO_POSITION;
    // Pixel distance, along y-axis, from the center of the recycler view to the nearest child.
    private float mEdgeToCenterDistance = Float.MAX_VALUE;
    private float mCardCenterToScreenCenterDistancePx = Float.MAX_VALUE;
    // When card data is loaded, this many cards should be animated as data is bound to them.
    private int mNumCardsToAnimate;
    // When card data is loaded, this is the position of the leftmost card to be animated.
    private int mCardAnimationStartPosition;
    // When card data is loaded, the animations may be delayed so that other animations can complete
    private int mExtraAnimationDelay;

    interface OnSelectionListener {
        /**
         * The card was moved to the center, thus selecting it.
         */
        void onCardSelected(@NonNull WalletCardViewInfo card);

        /**
         * The card was clicked.
         */
        void onCardClicked(@NonNull WalletCardViewInfo card);
    }

    interface OnCardScrollListener {
        void onCardScroll(WalletCardViewInfo centerCard, WalletCardViewInfo nextCard,
                float percentDistanceFromCenter);
    }

    public WalletCardCarousel(Context context) {
        this(context, null);
    }

    public WalletCardCarousel(Context context, @Nullable AttributeSet attributeSet) {
        super(context, attributeSet);
        setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        mScreenWidth = Math.min(metrics.widthPixels, metrics.heightPixels);
        mCardWidthPx = Math.round(mScreenWidth * CARD_SCREEN_WIDTH_RATIO);
        mCardHeightPx = Math.round(mCardWidthPx / CARD_ASPECT_RATIO);
        mCornerRadiusPx = mCardWidthPx * CORNER_RADIUS_RATIO;
        mCardMarginPx = Math.round(mScreenWidth * CARD_MARGIN_RATIO);
        mTotalCardWidth =
                mCardWidthPx + getResources().getDimensionPixelSize(R.dimen.card_margin) * 2;
        mCardEdgeToCenterDistance = mTotalCardWidth / 2f;
        addOnScrollListener(new CardCarouselScrollListener());
        new CarouselSnapHelper().attachToRecyclerView(this);
        mWalletCardAdapter = new WalletCardAdapter();
        mWalletCardAdapter.setHasStableIds(true);
        setAdapter(mWalletCardAdapter);
        ViewCompat.setAccessibilityDelegate(this, new CardCarouselAccessibilityDelegate(this));
        updatePadding(mScreenWidth);
    }

    @Override
    public void onViewAdded(View child) {
        super.onViewAdded(child);
        LayoutParams layoutParams = (LayoutParams) child.getLayoutParams();
        layoutParams.leftMargin = mCardMarginPx;
        layoutParams.rightMargin = mCardMarginPx;
        child.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> updateCardView(child));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        int width = getWidth();
        if (mWalletCardAdapter.getItemCount() > 1) {
            // Whole carousel is opted out from system gesture.
            mSystemGestureExclusionZone.set(0, 0, width, getHeight());
            setSystemGestureExclusionRects(Collections.singletonList(mSystemGestureExclusionZone));
        }
        if (width != mScreenWidth) {
            updatePadding(width);
        }
    }

    /**
     * The padding pushes the first and last cards in the list to the center when they are
     * selected.
     */
    private void updatePadding(int viewWidth) {
        int paddingHorizontal = (viewWidth - mTotalCardWidth) / 2 - mCardMarginPx;
        paddingHorizontal = Math.max(0, paddingHorizontal); // just in case
        setPadding(paddingHorizontal, getPaddingTop(), paddingHorizontal, getPaddingBottom());

        // re-center selected card after changing padding (if card is selected)
        if (mWalletCardAdapter != null
                && mWalletCardAdapter.getItemCount() > 0
                && mCenteredAdapterPosition != NO_POSITION) {
            ViewHolder viewHolder = findViewHolderForAdapterPosition(mCenteredAdapterPosition);
            if (viewHolder != null) {
                View cardView = viewHolder.itemView;
                int cardCenter = (cardView.getLeft() + cardView.getRight()) / 2;
                int viewCenter = (getLeft() + getRight()) / 2;
                int scrollX = cardCenter - viewCenter;
                scrollBy(scrollX, 0);
            }
        }
    }

    void setSelectionListener(OnSelectionListener selectionListener) {
        mSelectionListener = selectionListener;
    }

    void setCardScrollListener(OnCardScrollListener scrollListener) {
        mCardScrollListener = scrollListener;
    }

    int getCardWidthPx() {
        return mCardWidthPx;
    }

    int getCardHeightPx() {
        return mCardHeightPx;
    }

    /**
     * Set card data. Returns true if carousel was empty, indicating that views will be animated
     */
    boolean setData(List<WalletCardViewInfo> data, int selectedIndex) {
        boolean wasEmpty = mWalletCardAdapter.getItemCount() == 0;
        mWalletCardAdapter.setData(data);
        if (wasEmpty) {
            scrollToPosition(selectedIndex);
            mNumCardsToAnimate = numCardsOnScreen(data.size(), selectedIndex);
            mCardAnimationStartPosition = Math.max(selectedIndex - 1, 0);
        }
        WalletCardViewInfo selectedCard = data.get(selectedIndex);
        mCardScrollListener.onCardScroll(selectedCard, selectedCard, 0);
        return wasEmpty;
    }

    @Override
    public void scrollToPosition(int position) {
        super.scrollToPosition(position);
        mSelectionListener.onCardSelected(mWalletCardAdapter.mData.get(position));
    }

    /**
     * The number of cards shown on screen when one of the cards is position in the center. This is
     * also the num
     */
    private static int numCardsOnScreen(int numCards, int selectedIndex) {
        if (numCards <= 2) {
            return numCards;
        }
        // When there are 3 or more cards, 3 cards will be shown unless the first or last card is
        // centered on screen.
        return selectedIndex > 0 && selectedIndex < (numCards - 1) ? 3 : 2;
    }

    private void updateCardView(View view) {
        WalletCardViewHolder viewHolder = (WalletCardViewHolder) view.getTag();
        CardView cardView = viewHolder.cardView;
        float center = (float) getWidth() / 2f;
        float viewCenter = (view.getRight() + view.getLeft()) / 2f;
        float viewWidth = view.getWidth();
        float position = (viewCenter - center) / viewWidth;
        float scaleFactor = Math.max(UNSELECTED_CARD_SCALE, 1f - Math.abs(position));

        cardView.setScaleX(scaleFactor);
        cardView.setScaleY(scaleFactor);

        // a card is the "centered card" until its edge has moved past the center of the recycler
        // view. note that we also need to factor in the negative margin.
        // Find the edge that is closer to the center.
        int edgePosition =
                viewCenter < center ? view.getRight() + mCardMarginPx
                        : view.getLeft() - mCardMarginPx;

        if (Math.abs(viewCenter - center) < mCardCenterToScreenCenterDistancePx) {
            int childAdapterPosition = getChildAdapterPosition(view);
            if (childAdapterPosition == RecyclerView.NO_POSITION) {
                return;
            }
            mCenteredAdapterPosition = getChildAdapterPosition(view);
            mEdgeToCenterDistance = edgePosition - center;
            mCardCenterToScreenCenterDistancePx = Math.abs(viewCenter - center);
        }
    }

    void setExtraAnimationDelay(int extraAnimationDelay) {
        mExtraAnimationDelay = extraAnimationDelay;
    }

    private class CardCarouselScrollListener extends OnScrollListener {

        private int oldState = -1;

        @Override
        public void onScrollStateChanged(@NonNull RecyclerView recyclerView, int newState) {
            if (newState == RecyclerView.SCROLL_STATE_IDLE && newState != oldState) {
                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
            }
            oldState = newState;
        }

        /**
         * Callback method to be invoked when the RecyclerView has been scrolled. This will be
         * called after the scroll has completed.
         *
         * <p>This callback will also be called if visible item range changes after a layout
         * calculation. In that case, dx and dy will be 0.
         *
         * @param recyclerView The RecyclerView which scrolled.
         * @param dx           The amount of horizontal scroll.
         * @param dy           The amount of vertical scroll.
         */
        @Override
        public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
            mCenteredAdapterPosition = RecyclerView.NO_POSITION;
            mEdgeToCenterDistance = Float.MAX_VALUE;
            mCardCenterToScreenCenterDistancePx = Float.MAX_VALUE;
            for (int i = 0; i < getChildCount(); i++) {
                updateCardView(getChildAt(i));
            }
            if (mCenteredAdapterPosition == RecyclerView.NO_POSITION || dx == 0) {
                return;
            }

            int nextAdapterPosition =
                    mCenteredAdapterPosition + (mEdgeToCenterDistance > 0 ? 1 : -1);
            if (nextAdapterPosition < 0 || nextAdapterPosition >= mWalletCardAdapter.mData.size()) {
                return;
            }

            // Update the label text based on the currently selected card and the next one
            WalletCardViewInfo centerCard = mWalletCardAdapter.mData.get(mCenteredAdapterPosition);
            WalletCardViewInfo nextCard = mWalletCardAdapter.mData.get(nextAdapterPosition);
            float percentDistanceFromCenter =
                    Math.abs(mEdgeToCenterDistance) / mCardEdgeToCenterDistance;
            mCardScrollListener.onCardScroll(centerCard, nextCard, percentDistanceFromCenter);
        }
    }

    private class CarouselSnapHelper extends PagerSnapHelper {

        private static final float MILLISECONDS_PER_INCH = 200.0F;
        private static final int MAX_SCROLL_ON_FLING_DURATION = 80; // ms

        @Override
        public View findSnapView(LayoutManager layoutManager) {
            View view = super.findSnapView(layoutManager);
            if (view == null) {
                // implementation decides not to snap
                return null;
            }
            WalletCardViewHolder viewHolder = (WalletCardViewHolder) view.getTag();
            WalletCardViewInfo card = viewHolder.info;
            mSelectionListener.onCardSelected(card);
            mCardScrollListener.onCardScroll(card, card, 0);
            return view;
        }

        /**
         * The default SnapScroller is a little sluggish
         */
        @Override
        protected LinearSmoothScroller createSnapScroller(LayoutManager layoutManager) {
            return new LinearSmoothScroller(getContext()) {
                @Override
                protected void onTargetFound(View targetView, State state, Action action) {
                    int[] snapDistances = calculateDistanceToFinalSnap(layoutManager, targetView);
                    final int dx = snapDistances[0];
                    final int dy = snapDistances[1];
                    final int time = calculateTimeForDeceleration(
                            Math.max(Math.abs(dx), Math.abs(dy)));
                    if (time > 0) {
                        action.update(dx, dy, time, mDecelerateInterpolator);
                    }
                }

                @Override
                protected float calculateSpeedPerPixel(DisplayMetrics displayMetrics) {
                    return MILLISECONDS_PER_INCH / displayMetrics.densityDpi;
                }

                @Override
                protected int calculateTimeForScrolling(int dx) {
                    return Math.min(MAX_SCROLL_ON_FLING_DURATION,
                            super.calculateTimeForScrolling(dx));
                }
            };
        }
    }

    private static class WalletCardViewHolder extends ViewHolder {

        private final CardView cardView;
        private final ImageView imageView;
        private WalletCardViewInfo info;

        WalletCardViewHolder(View view) {
            super(view);
            cardView = view.requireViewById(R.id.card);
            imageView = cardView.requireViewById(R.id.image);
        }
    }

    private class WalletCardAdapter extends Adapter<WalletCardViewHolder> {

        private List<WalletCardViewInfo> mData = Collections.emptyList();

        @Override
        public int getItemViewType(int position) {
            return 0;
        }

        @NonNull
        @Override
        public WalletCardViewHolder onCreateViewHolder(
                @NonNull ViewGroup parent, int itemViewType) {
            LayoutInflater inflater = LayoutInflater.from(getContext());
            View view = inflater.inflate(R.layout.wallet_card_view, parent, false);
            WalletCardViewHolder viewHolder = new WalletCardViewHolder(view);
            CardView cardView = viewHolder.cardView;
            cardView.setRadius(mCornerRadiusPx);
            ViewGroup.LayoutParams layoutParams = cardView.getLayoutParams();
            layoutParams.width = mCardWidthPx;
            layoutParams.height = mCardHeightPx;
            view.setTag(viewHolder);
            return viewHolder;
        }

        @Override
        public void onBindViewHolder(WalletCardViewHolder viewHolder, int position) {
            WalletCardViewInfo info = mData.get(position);
            viewHolder.info = info;
            viewHolder.imageView.setImageDrawable(info.getCardDrawable());
            viewHolder.cardView.setContentDescription(info.getContentDescription());
            viewHolder.cardView.setOnClickListener(
                    v -> {
                        if (position != mCenteredAdapterPosition) {
                            smoothScrollToPosition(position);
                        } else {
                            mSelectionListener.onCardClicked(info);
                        }
                    });
            if (mNumCardsToAnimate > 0 && (position - mCardAnimationStartPosition < 2)) {
                mNumCardsToAnimate--;
                // Animation of cards is progressively delayed from left to right in 50ms increments
                // Additional delay may be added if the empty state view needs to be animated first.
                int startDelay = (position - mCardAnimationStartPosition) * CARD_ANIM_ALPHA_DELAY
                        + mExtraAnimationDelay;
                viewHolder.itemView.setAlpha(0f);
                viewHolder.itemView.animate().alpha(1f)
                        .setStartDelay(Math.max(0, startDelay))
                        .setDuration(CARD_ANIM_ALPHA_DURATION).start();
            }
        }

        @Override
        public long getItemId(int position) {
            return mData.get(position).getCardId().hashCode();
        }

        @Override
        public int getItemCount() {
            return mData.size();
        }

        private void setData(List<WalletCardViewInfo> data) {
            this.mData = data;
            notifyDataSetChanged();
        }
    }

    private class CardCarouselAccessibilityDelegate extends RecyclerViewAccessibilityDelegate {

        private CardCarouselAccessibilityDelegate(@NonNull RecyclerView recyclerView) {
            super(recyclerView);
        }

        @Override
        public boolean onRequestSendAccessibilityEvent(
                ViewGroup viewGroup, View view, AccessibilityEvent accessibilityEvent) {
            int eventType = accessibilityEvent.getEventType();
            if (eventType == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
                scrollToPosition(getChildAdapterPosition(view));
            }
            return super.onRequestSendAccessibilityEvent(viewGroup, view, accessibilityEvent);
        }
    }
}
