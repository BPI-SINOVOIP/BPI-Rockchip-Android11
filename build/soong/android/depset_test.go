// Copyright 2020 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package android

import (
	"fmt"
	"reflect"
	"strings"
	"testing"
)

func ExampleDepSet_ToList_postordered() {
	a := NewDepSetBuilder(POSTORDER).Direct(PathForTesting("a")).Build()
	b := NewDepSetBuilder(POSTORDER).Direct(PathForTesting("b")).Transitive(a).Build()
	c := NewDepSetBuilder(POSTORDER).Direct(PathForTesting("c")).Transitive(a).Build()
	d := NewDepSetBuilder(POSTORDER).Direct(PathForTesting("d")).Transitive(b, c).Build()

	fmt.Println(d.ToList().Strings())
	// Output: [a b c d]
}

func ExampleDepSet_ToList_preordered() {
	a := NewDepSetBuilder(PREORDER).Direct(PathForTesting("a")).Build()
	b := NewDepSetBuilder(PREORDER).Direct(PathForTesting("b")).Transitive(a).Build()
	c := NewDepSetBuilder(PREORDER).Direct(PathForTesting("c")).Transitive(a).Build()
	d := NewDepSetBuilder(PREORDER).Direct(PathForTesting("d")).Transitive(b, c).Build()

	fmt.Println(d.ToList().Strings())
	// Output: [d b a c]
}

func ExampleDepSet_ToList_topological() {
	a := NewDepSetBuilder(TOPOLOGICAL).Direct(PathForTesting("a")).Build()
	b := NewDepSetBuilder(TOPOLOGICAL).Direct(PathForTesting("b")).Transitive(a).Build()
	c := NewDepSetBuilder(TOPOLOGICAL).Direct(PathForTesting("c")).Transitive(a).Build()
	d := NewDepSetBuilder(TOPOLOGICAL).Direct(PathForTesting("d")).Transitive(b, c).Build()

	fmt.Println(d.ToList().Strings())
	// Output: [d b c a]
}

func ExampleDepSet_ToSortedList() {
	a := NewDepSetBuilder(POSTORDER).Direct(PathForTesting("a")).Build()
	b := NewDepSetBuilder(POSTORDER).Direct(PathForTesting("b")).Transitive(a).Build()
	c := NewDepSetBuilder(POSTORDER).Direct(PathForTesting("c")).Transitive(a).Build()
	d := NewDepSetBuilder(POSTORDER).Direct(PathForTesting("d")).Transitive(b, c).Build()

	fmt.Println(d.ToSortedList().Strings())
	// Output: [a b c d]
}

// Tests based on Bazel's ExpanderTestBase.java to ensure compatibility
// https://github.com/bazelbuild/bazel/blob/master/src/test/java/com/google/devtools/build/lib/collect/nestedset/ExpanderTestBase.java
func TestDepSet(t *testing.T) {
	a := PathForTesting("a")
	b := PathForTesting("b")
	c := PathForTesting("c")
	c2 := PathForTesting("c2")
	d := PathForTesting("d")
	e := PathForTesting("e")

	tests := []struct {
		name                             string
		depSet                           func(t *testing.T, order DepSetOrder) *DepSet
		postorder, preorder, topological []string
	}{
		{
			name: "simple",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				return NewDepSet(order, Paths{c, a, b}, nil)
			},
			postorder:   []string{"c", "a", "b"},
			preorder:    []string{"c", "a", "b"},
			topological: []string{"c", "a", "b"},
		},
		{
			name: "simpleNoDuplicates",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				return NewDepSet(order, Paths{c, a, a, a, b}, nil)
			},
			postorder:   []string{"c", "a", "b"},
			preorder:    []string{"c", "a", "b"},
			topological: []string{"c", "a", "b"},
		},
		{
			name: "nesting",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				subset := NewDepSet(order, Paths{c, a, e}, nil)
				return NewDepSet(order, Paths{b, d}, []*DepSet{subset})
			},
			postorder:   []string{"c", "a", "e", "b", "d"},
			preorder:    []string{"b", "d", "c", "a", "e"},
			topological: []string{"b", "d", "c", "a", "e"},
		},
		{
			name: "builderReuse",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				assertEquals := func(t *testing.T, w, g Paths) {
					if !reflect.DeepEqual(w, g) {
						t.Errorf("want %q, got %q", w, g)
					}
				}
				builder := NewDepSetBuilder(order)
				assertEquals(t, nil, builder.Build().ToList())

				builder.Direct(b)
				assertEquals(t, Paths{b}, builder.Build().ToList())

				builder.Direct(d)
				assertEquals(t, Paths{b, d}, builder.Build().ToList())

				child := NewDepSetBuilder(order).Direct(c, a, e).Build()
				builder.Transitive(child)
				return builder.Build()
			},
			postorder:   []string{"c", "a", "e", "b", "d"},
			preorder:    []string{"b", "d", "c", "a", "e"},
			topological: []string{"b", "d", "c", "a", "e"},
		},
		{
			name: "builderChaining",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				return NewDepSetBuilder(order).Direct(b).Direct(d).
					Transitive(NewDepSetBuilder(order).Direct(c, a, e).Build()).Build()
			},
			postorder:   []string{"c", "a", "e", "b", "d"},
			preorder:    []string{"b", "d", "c", "a", "e"},
			topological: []string{"b", "d", "c", "a", "e"},
		},
		{
			name: "transitiveDepsHandledSeparately",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				subset := NewDepSetBuilder(order).Direct(c, a, e).Build()
				builder := NewDepSetBuilder(order)
				// The fact that we add the transitive subset between the Direct(b) and Direct(d)
				// calls should not change the result.
				builder.Direct(b)
				builder.Transitive(subset)
				builder.Direct(d)
				return builder.Build()
			},
			postorder:   []string{"c", "a", "e", "b", "d"},
			preorder:    []string{"b", "d", "c", "a", "e"},
			topological: []string{"b", "d", "c", "a", "e"},
		},
		{
			name: "nestingNoDuplicates",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				subset := NewDepSetBuilder(order).Direct(c, a, e).Build()
				return NewDepSetBuilder(order).Direct(b, d, e).Transitive(subset).Build()
			},
			postorder:   []string{"c", "a", "e", "b", "d"},
			preorder:    []string{"b", "d", "e", "c", "a"},
			topological: []string{"b", "d", "c", "a", "e"},
		},
		{
			name: "chain",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				c := NewDepSetBuilder(order).Direct(c).Build()
				b := NewDepSetBuilder(order).Direct(b).Transitive(c).Build()
				a := NewDepSetBuilder(order).Direct(a).Transitive(b).Build()

				return a
			},
			postorder:   []string{"c", "b", "a"},
			preorder:    []string{"a", "b", "c"},
			topological: []string{"a", "b", "c"},
		},
		{
			name: "diamond",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				d := NewDepSetBuilder(order).Direct(d).Build()
				c := NewDepSetBuilder(order).Direct(c).Transitive(d).Build()
				b := NewDepSetBuilder(order).Direct(b).Transitive(d).Build()
				a := NewDepSetBuilder(order).Direct(a).Transitive(b).Transitive(c).Build()

				return a
			},
			postorder:   []string{"d", "b", "c", "a"},
			preorder:    []string{"a", "b", "d", "c"},
			topological: []string{"a", "b", "c", "d"},
		},
		{
			name: "extendedDiamond",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				d := NewDepSetBuilder(order).Direct(d).Build()
				e := NewDepSetBuilder(order).Direct(e).Build()
				b := NewDepSetBuilder(order).Direct(b).Transitive(d).Transitive(e).Build()
				c := NewDepSetBuilder(order).Direct(c).Transitive(e).Transitive(d).Build()
				a := NewDepSetBuilder(order).Direct(a).Transitive(b).Transitive(c).Build()
				return a
			},
			postorder:   []string{"d", "e", "b", "c", "a"},
			preorder:    []string{"a", "b", "d", "e", "c"},
			topological: []string{"a", "b", "c", "e", "d"},
		},
		{
			name: "extendedDiamondRightArm",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				d := NewDepSetBuilder(order).Direct(d).Build()
				e := NewDepSetBuilder(order).Direct(e).Build()
				b := NewDepSetBuilder(order).Direct(b).Transitive(d).Transitive(e).Build()
				c2 := NewDepSetBuilder(order).Direct(c2).Transitive(e).Transitive(d).Build()
				c := NewDepSetBuilder(order).Direct(c).Transitive(c2).Build()
				a := NewDepSetBuilder(order).Direct(a).Transitive(b).Transitive(c).Build()
				return a
			},
			postorder:   []string{"d", "e", "b", "c2", "c", "a"},
			preorder:    []string{"a", "b", "d", "e", "c", "c2"},
			topological: []string{"a", "b", "c", "c2", "e", "d"},
		},
		{
			name: "orderConflict",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				child1 := NewDepSetBuilder(order).Direct(a, b).Build()
				child2 := NewDepSetBuilder(order).Direct(b, a).Build()
				parent := NewDepSetBuilder(order).Transitive(child1).Transitive(child2).Build()
				return parent
			},
			postorder:   []string{"a", "b"},
			preorder:    []string{"a", "b"},
			topological: []string{"b", "a"},
		},
		{
			name: "orderConflictNested",
			depSet: func(t *testing.T, order DepSetOrder) *DepSet {
				a := NewDepSetBuilder(order).Direct(a).Build()
				b := NewDepSetBuilder(order).Direct(b).Build()
				child1 := NewDepSetBuilder(order).Transitive(a).Transitive(b).Build()
				child2 := NewDepSetBuilder(order).Transitive(b).Transitive(a).Build()
				parent := NewDepSetBuilder(order).Transitive(child1).Transitive(child2).Build()
				return parent
			},
			postorder:   []string{"a", "b"},
			preorder:    []string{"a", "b"},
			topological: []string{"b", "a"},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			t.Run("postorder", func(t *testing.T) {
				depSet := tt.depSet(t, POSTORDER)
				if g, w := depSet.ToList().Strings(), tt.postorder; !reflect.DeepEqual(g, w) {
					t.Errorf("expected ToList() = %q, got %q", w, g)
				}
			})
			t.Run("preorder", func(t *testing.T) {
				depSet := tt.depSet(t, PREORDER)
				if g, w := depSet.ToList().Strings(), tt.preorder; !reflect.DeepEqual(g, w) {
					t.Errorf("expected ToList() = %q, got %q", w, g)
				}
			})
			t.Run("topological", func(t *testing.T) {
				depSet := tt.depSet(t, TOPOLOGICAL)
				if g, w := depSet.ToList().Strings(), tt.topological; !reflect.DeepEqual(g, w) {
					t.Errorf("expected ToList() = %q, got %q", w, g)
				}
			})
		})
	}
}

func TestDepSetInvalidOrder(t *testing.T) {
	orders := []DepSetOrder{POSTORDER, PREORDER, TOPOLOGICAL}

	run := func(t *testing.T, order1, order2 DepSetOrder) {
		defer func() {
			if r := recover(); r != nil {
				if err, ok := r.(error); !ok {
					t.Fatalf("expected panic error, got %v", err)
				} else if !strings.Contains(err.Error(), "incompatible order") {
					t.Fatalf("expected incompatible order error, got %v", err)
				}
			}
		}()
		NewDepSet(order1, nil, []*DepSet{NewDepSet(order2, nil, nil)})
		t.Fatal("expected panic")
	}

	for _, order1 := range orders {
		t.Run(order1.String(), func(t *testing.T) {
			for _, order2 := range orders {
				t.Run(order2.String(), func(t *testing.T) {
					if order1 != order2 {
						run(t, order1, order2)
					}
				})
			}
		})
	}
}
