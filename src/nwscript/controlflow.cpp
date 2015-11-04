/* xoreos - A reimplementation of BioWare's Aurora engine
 *
 * xoreos is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  Higher-level control flow analysis on NWScript bytecode.
 */

#ifndef NWSCRIPT_CONTROLFLOW_H
#define NWSCRIPT_CONTROLFLOW_H

#include <cassert>

#include <algorithm>

#include "src/common/util.h"
#include "src/common/error.h"

#include "src/nwscript/controlflow.h"
#include "src/nwscript/instruction.h"
#include "src/nwscript/block.h"
#include "src/nwscript/subroutine.h"

namespace NWScript {

/** Does this block have only one instruction? */
static bool isSingularBlock(const Block &block) {
	return block.instructions.size() == 1;
}

/** Is this an independant block that consists of a single JMP?
 *
 *  A dependant block is one that has only parents that unconditionally seemlessy jump
 *  to this block. Essentially, the block has only been divided because a third
 *  block jumps into its middle.
 */
static bool isLoneJump(const Block *block) {
	if (!block)
		return false;

	const bool loneJump = isSingularBlock(*block) && (block->instructions[0]->opcode == kOpcodeJMP);

	bool independ = false;
	for (std::vector<const Block *>::const_iterator p = block->parents.begin();
	     p != block->parents.end(); ++p) {

		if ((*p)->hasConditionalChildren()) {
			independ = true;
			break;
		}
	}

	return loneJump && independ;
}

/** Is this a block that *doesn't* consists of a single JMP or isn't independant? */
static bool isNotLoneJump(const Block *block) {
	return !isLoneJump(block);
}

/** Is this a block that has a return instruction? */
static bool isReturnBlock(const Block &block) {
	for (std::vector<const Instruction *>::const_iterator i = block.instructions.begin();
	     i != block.instructions.end(); i++)
		if ((*i)->opcode == kOpcodeRETN)
			return true;

	return false;
}

/** Return the block that has the earliest, lowest address. */
static const Block *getEarliestBlock(const std::vector<const Block *> &blocks) {
	const Block *result = 0;
	for (std::vector<const Block *>::const_iterator b = blocks.begin(); b != blocks.end(); ++b)
		if (!result || ((*b)->address < result->address))
			result = *b;

	return result;
}

/** Return the block that has the latest, largest address. */
static const Block *getLatestBlock(const std::vector<const Block *> &blocks) {
	const Block *result = 0;
	for (std::vector<const Block *>::const_iterator b = blocks.begin(); b != blocks.end(); ++b)
		if (!result || ((*b)->address > result->address))
			result = *b;

	return result;
}

static void findPathMerge(std::vector<const Block *> &merges, const Block &block1, const Block &block2) {
	if (block1.address > block2.address)
		return;

	if (hasLinearPath(block1, block2)) {
		merges.push_back(&block2);
		return;
	}

	for (std::vector<const Block *>::const_iterator c = block2.children.begin();
	     c != block2.children.end(); ++c)
		findPathMerge(merges, block1, **c);
}

/** Find the block where the path of these two blocks come back together. */
static const Block *findPathMerge(const Block &block1, const Block &block2) {
	std::vector<const Block *> merges;

	if (block1.address < block2.address)
		findPathMerge(merges, block1, block2);
	else
		findPathMerge(merges, block2, block1);

	return getEarliestBlock(merges);
}


static void detectDoWhile(Blocks &blocks) {
	/* Find all do-while loops. A do-while loop has a tail block that
	 * only has a single JMP that jumps back to the loop head. */

	for (Blocks::iterator head = blocks.begin(); head != blocks.end(); ++head) {
		// Find all parents of this block from later in the script that only consist of a single JMP.

		std::vector<const Block *> parents = head->getLaterParents();
		parents.erase(std::remove_if(parents.begin(), parents.end(), isNotLoneJump), parents.end());

		// Get the parent thas has the highest address and make sure it's still undetermined
		Block *tail = const_cast<Block *>(getLatestBlock(parents));
		if (!tail || tail->hasMainControl())
			continue;

		Block *next = const_cast<Block *>(getNextBlock(blocks, *tail));
		if (!next)
			throw Common::Exception("Can't find a block following the do-while loop");

		// If such a parent exists, it's the tail of a do-while loop
		head->controls.push_back(ControlStructure(kControlTypeDoWhileHead, *head, *tail, *next));
		tail->controls.push_back(ControlStructure(kControlTypeDoWhileTail, *head, *tail, *next));
		next->controls.push_back(ControlStructure(kControlTypeDoWhileNext, *head, *tail, *next));
	}
}

static void detectWhile(Blocks &blocks) {
	/* Find all while loops. A while loop has a tail block that isn't a
	 * do-while loop tail, tha jumps back to the loop head. */

	for (Blocks::iterator head = blocks.begin(); head != blocks.end(); ++head) {
		// Find all parents of this block from later in the script

		std::vector<const Block *> parents = head->getLaterParents();

		// Get the parent thas has the highest address and make sure it's still undetermined
		Block *tail = const_cast<Block *>(getLatestBlock(parents));
		if (!tail || tail ->hasMainControl())
			continue;

		Block *next = const_cast<Block *>(getNextBlock(blocks, *tail));
		if (!next)
			throw Common::Exception("Can't find a block following the do-while loop");

		// If such a parent exists, it's the tail of a while loop
		head->controls.push_back(ControlStructure(kControlTypeWhileHead, *head, *tail, *next));
		tail->controls.push_back(ControlStructure(kControlTypeWhileTail, *head, *tail, *next));
		next->controls.push_back(ControlStructure(kControlTypeWhileNext, *head, *tail, *next));
	}
}

static void detectBreak(Blocks &blocks) {
	/* Find all "break;" statements. A break is created by a block that
	 * only contains a single JMP that jumps directly outside the loop. */

	for (Blocks::iterator b = blocks.begin(); b != blocks.end(); ++b) {
		// Find all undetermined blocks that consist of a single JMP
		if (b->hasMainControl() || !isLoneJump(&*b))
			continue;

		// Make sure they jump to a block that directly follows a loop
		if ((b->children.size() != 1) || !b->children[0]->isLoopNext())
			continue;

		// Get the loop blocks
		const Block *head = 0, *tail = 0, *next = 0;
		if (!b->children[0]->getLoop(head, tail, next))
			continue;

		// Mark the block as being a loop break
		b->controls.push_back(ControlStructure(kControlTypeBreak, *head, *tail, *next));
	}
}

static void detectContinue(Blocks &blocks) {
	/* Find all "continue;" statements. A continue is created by a block that
	 * only contains a single JMP that jumps directly to the tail of the loop. */

	for (Blocks::iterator b = blocks.begin(); b != blocks.end(); ++b) {
		// Find all undetermined blocks that consist of a single JMP
		if (b->hasMainControl() || !isLoneJump(&*b))
			continue;

		// Make sure they jump to a loop tail
		if ((b->children.size() != 1) || !b->children[0]->isLoopTail())
			continue;

		// Get the loop blocks
		const Block *head = 0, *tail = 0, *next = 0;
		if (!b->children[0]->getLoop(head, tail, next))
			continue;

		// Mark the block as being a loop continue
		b->controls.push_back(ControlStructure(kControlTypeContinue, *head, *tail, *next));
	}
}

static void detectReturn(Blocks &blocks) {
	/* Find all "return;" (and "return $value;") statements. A return block is
	 * a block that contains a RETN statement, or that unconditionally jumps
	 * to a block with a RETN statement. */

	for (Blocks::iterator b = blocks.begin(); b != blocks.end(); ++b) {
		// Find all undetermined blocks with a RETN
		if (b->hasMainControl() || !isReturnBlock(*b))
			continue;

		// Make sure this is not the entry (and only) block in this subroutine
		if (!b->subRoutine || (b->subRoutine->address == b->address))
			continue;

		bool hasReturnParent = false;

		if (isSingularBlock(*b)) {
			/* If this is a block that has *only* a RETN, this block is
			 * probably a shared RETN used by several "return;" statements. */

			for (std::vector<const Block *>::const_iterator p = b->parents.begin();
			     p != b->parents.end(); ++p) {

				if ((*p)->hasUnconditionalChildren() && !(*p)->hasMainControl()) {
					hasReturnParent = true;
					const_cast<Block *>(*p)->controls.push_back(ControlStructure(kControlTypeReturn, *b));
				}
			}
		}

		// If we haven't marked any of this block's parents, mark this block instead
		if (!hasReturnParent)
			b->controls.push_back(ControlStructure(kControlTypeReturn, *b));
	}
}

static void detectIf(Blocks &blocks) {
	/* Detect if and if-else statements. An if starts with a yet undetermined block
	 * that contains a conditional jump (JZ or JNZ). */

	for (Blocks::iterator ifCond = blocks.begin(); ifCond != blocks.end(); ++ifCond) {
		// Find all undetermined blocks (but while heads are okay, too)
		if (ifCond->hasMainControl() && !ifCond->isControl(kControlTypeWhileHead))
			continue;

		// They do need to have conditionals, though
		if ((ifCond->children.size() != 2) || !ifCond->hasConditionalChildren())
			continue;

		// If there's no direct linear path between the two branches, this is an if-else
		const bool isIfElse = !hasLinearPath(*ifCond->children[0], *ifCond->children[1]);

		Block *ifTrue = 0, *ifElse = 0, *ifNext = 0;

		if (isIfElse) {
			// The two branches are the if and the else
			ifTrue = const_cast<Block *>(ifCond->children[0]);
			ifElse = const_cast<Block *>(ifCond->children[1]);

			// If we have both, try to find the block where the code flow unites again
			if (ifTrue && ifElse)
				ifNext = const_cast<Block *>(findPathMerge(*ifTrue, *ifElse));

		} else {
			// The if branch has the smaller address, and the flow continues at the larger address
			const bool firstSmaller = ifCond->children[0]->address < ifCond->children[1]->address;

			const Block *low  = firstSmaller ? ifCond->children[0] : ifCond->children[1];
			const Block *high = firstSmaller ? ifCond->children[1] : ifCond->children[0];

			ifTrue = const_cast<Block *>(low);
			ifNext = const_cast<Block *>(high);
		}

		assert(ifTrue);

		// Mark the conditional and the true branch
		ifCond->controls.push_back(ControlStructure(kControlTypeIfCond, *ifCond, *ifTrue, ifElse, ifNext));
		ifTrue->controls.push_back(ControlStructure(kControlTypeIfTrue, *ifCond, *ifTrue, ifElse, ifNext));

		// If we have an else and/or a next branch, mark them as well
		if (ifElse)
			ifElse->controls.push_back(ControlStructure(kControlTypeIfElse, *ifCond, *ifTrue, ifElse, ifNext));
		if (ifNext)
			ifNext->controls.push_back(ControlStructure(kControlTypeIfNext, *ifCond, *ifTrue, ifElse, ifNext));
	}
}


void analyzeControlFlow(Blocks &blocks) {
	/* Detect the different control structures. The order is important! */

	detectDoWhile (blocks);
	detectWhile   (blocks);
	detectBreak   (blocks);
	detectContinue(blocks);
	detectReturn  (blocks);
	detectIf      (blocks);
}

} // End of namespace NWScript

#endif // NWSCRIPT_CONTROLFLOW_H