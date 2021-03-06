/*
 * Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_GC_SHARED_CARDTABLEMODREFBS_HPP
#define SHARE_VM_GC_SHARED_CARDTABLEMODREFBS_HPP

#include "gc/shared/modRefBarrierSet.hpp"
#include "utilities/align.hpp"

class CardTable;

// This kind of "BarrierSet" allows a "CollectedHeap" to detect and
// enumerate ref fields that have been modified (since the last
// enumeration.)

// As it currently stands, this barrier is *imprecise*: when a ref field in
// an object "o" is modified, the card table entry for the card containing
// the head of "o" is dirtied, not necessarily the card containing the
// modified field itself.  For object arrays, however, the barrier *is*
// precise; only the card containing the modified element is dirtied.
// Closures used to scan dirty cards should take these
// considerations into account.

class CardTableModRefBS: public ModRefBarrierSet {
  // Some classes get to look at some private stuff.
  friend class VMStructs;
 protected:

  // Used in support of ReduceInitialCardMarks; only consulted if COMPILER2
  // or INCLUDE_JVMCI is being used
  bool       _defer_initial_card_mark;
  CardTable* _card_table;

  CardTableModRefBS(CardTable* card_table, const BarrierSet::FakeRtti& fake_rtti);

 public:
  CardTableModRefBS(CardTable* card_table);
  ~CardTableModRefBS();

  CardTable* card_table() const { return _card_table; }

  virtual void initialize();

  void write_region(MemRegion mr) {
    invalidate(mr);
  }

 protected:
  void write_ref_array_work(MemRegion mr);

 public:
  // Record a reference update. Note that these versions are precise!
  // The scanning code has to handle the fact that the write barrier may be
  // either precise or imprecise. We make non-virtual inline variants of
  // these functions here for performance.
  template <DecoratorSet decorators, typename T>
  void write_ref_field_post(T* field, oop newVal);

  virtual void invalidate(MemRegion mr);

  // ReduceInitialCardMarks
  void initialize_deferred_card_mark_barriers();

  // If the CollectedHeap was asked to defer a store barrier above,
  // this informs it to flush such a deferred store barrier to the
  // remembered set.
  void flush_deferred_card_mark_barrier(JavaThread* thread);

  // Can a compiler initialize a new object without store barriers?
  // This permission only extends from the creation of a new object
  // via a TLAB up to the first subsequent safepoint. If such permission
  // is granted for this heap type, the compiler promises to call
  // defer_store_barrier() below on any slow path allocation of
  // a new object for which such initializing store barriers will
  // have been elided. G1, like CMS, allows this, but should be
  // ready to provide a compensating write barrier as necessary
  // if that storage came out of a non-young region. The efficiency
  // of this implementation depends crucially on being able to
  // answer very efficiently in constant time whether a piece of
  // storage in the heap comes from a young region or not.
  // See ReduceInitialCardMarks.
  virtual bool can_elide_tlab_store_barriers() const {
    return true;
  }

  // If a compiler is eliding store barriers for TLAB-allocated objects,
  // we will be informed of a slow-path allocation by a call
  // to on_slowpath_allocation_exit() below. Such a call precedes the
  // initialization of the object itself, and no post-store-barriers will
  // be issued. Some heap types require that the barrier strictly follows
  // the initializing stores. (This is currently implemented by deferring the
  // barrier until the next slow-path allocation or gc-related safepoint.)
  // This interface answers whether a particular barrier type needs the card
  // mark to be thus strictly sequenced after the stores.
  virtual bool card_mark_must_follow_store() const;

  virtual void on_slowpath_allocation_exit(JavaThread* thread, oop new_obj);
  virtual void on_thread_detach(JavaThread* thread);

  virtual void make_parsable(JavaThread* thread) { flush_deferred_card_mark_barrier(thread); }

  virtual void print_on(outputStream* st) const;

  template <DecoratorSet decorators, typename BarrierSetT = CardTableModRefBS>
  class AccessBarrier: public ModRefBarrierSet::AccessBarrier<decorators, BarrierSetT> {};
};

template<>
struct BarrierSet::GetName<CardTableModRefBS> {
  static const BarrierSet::Name value = BarrierSet::CardTableModRef;
};

template<>
struct BarrierSet::GetType<BarrierSet::CardTableModRef> {
  typedef CardTableModRefBS type;
};

#endif // SHARE_VM_GC_SHARED_CARDTABLEMODREFBS_HPP
