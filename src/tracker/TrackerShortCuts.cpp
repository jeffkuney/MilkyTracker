/*
 *  tracker/TrackerShortCuts.cpp
 *
 *  Copyright 2008 Peter Barth
 *
 *  This file is part of Milkytracker.
 *
 *  Milkytracker is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Milkytracker is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Milkytracker.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 *  TrackerShortCuts.cpp
 *  MilkyTracker
 *
 *  Created by Peter Barth on Thu May 19 2005.
 *
 */

#include "Tracker.h"
#include "ControlIDs.h"
#include "Screen.h"
#include "Event.h"
#include "PlayerController.h"
#include "PlayerLogic.h"

#include "Container.h"
#include "ListBox.h"
#include "PatternEditorControl.h"

#include "ModuleEditor.h"
#include "PatternTools.h"
#include "TrackerConfig.h"

#include "InputControlListener.h"
#include "SectionInstruments.h"
#include "SectionTranspose.h"
#include "SectionDiskMenu.h"

void Tracker::sendNoteDownToPatternEditor(PPEvent* event, pp_int32 note, PatternEditorControl* patternEditorControl)
{
	PlayerController* playerController = this->playerController;

	pp_int32 i;
	
	if (note >= 1 && note != PatternTools::getNoteOffNote() /* Key Off */)
	{
		// get current channel from pattern editor (= channel to play)
		pp_int32 chn = patternEditorControl->getCurrentChannel();	
		
		// if this is a valid note
		pp_int32 ins = getInstrumentToPlay(note, playerController);
		
		if (ins < 0)
		{
			if (event)
				event->cancel();
			return;
		}
		
		bool record = (editMode == EditModeMilkyTracker ? screen->hasFocus(patternEditorControl) : recordMode);
		bool releasePlay = false;
		bool isLiveRecording = (playerController->isPlaying() || playerController->isPlayingPattern()) && record && shouldFollowSong();
		
		// when we're not live recording, we need to decide of we're editing
		if (!isLiveRecording)
		{
			releasePlay = !record;
		}
		// if we're live recording this is a "release" play, it means that the 
		// note will not be repeated on a key being pressed, it will stay on
		// 'till the key is released, but only when the current selected column
		// is the note column
		else
		{
			releasePlay = patternEditorControl->getCursorPosInner() == 0;
		}
		
		if (releasePlay)
		{
			// Somewhere editing of text in an edit field takes place,
			// in that case we don't want to be playing anything
			if (isActiveEditing())
				return;
			
			// take a look if this key is already pressed
			bool isPressed = false;
			for (i = 0; i < 128; i++)
			{
				if (keys[i].note == note)
				{
					isPressed = true;
					break;
				}
			}
			// this key is already pressed, we won't play that note again
			if (isPressed)
			{
				// If we're live recording, this event should not be routed anywhere else
				// it terminates HERE! (event shall not be routed to the pattern editor)
				if (isLiveRecording && event)
					event->cancel();
				return;
			}
			
			// if we're not recording, cycle through the channels
			// use jam-channels for playing if requested
			if (!isLiveRecording)
			{
				chn = playerController->getNextPlayingChannel(chn);
			}
			else
			{
				// Get next recording channel: The base for selection of the next channel is the current channel within the pattern editor
				chn = playerController->getNextRecordingChannel(chn);
			}
			
		}
		else
		{
			// The cursor in the pattern editor must be located in the note column, if not abort
			if (patternEditorControl->getCursorPosInner() != 0)
			{
				//event->cancel();
				return;
			}
			else
			{
				pp_int32 newIns = patternEditorControl->getPatternEditor()->getCurrentActiveInstrument();
				if (newIns >= 0)
					ins = newIns;
			}
		}
		
		
		// key is not pressed, play note and remember key + channel + position within module
		pp_int32 pos = -1, row = 0, ticker = 0;
		
		ASSERT(sizeof(mp_sint32) == sizeof(pp_int32));
		
		if (isLiveRecording && !playerController->isPlayingPattern())
			playerController->getPosition((mp_sint32&)pos, (mp_sint32&)row, (mp_sint32&)ticker);
		else if (isLiveRecording)
		{
			playerController->getPosition((mp_sint32&)pos, (mp_sint32&)row, (mp_sint32&)ticker);
			pos = -1;
		}
		
		if (chn != -1)
		{
			for (i = 0; i < 128; i++)
			{
				if (!keys[i].note)
				{
					keys[i].note = note;
					keys[i].ins = ins;
					keys[i].channel = chn;
					keys[i].pos = pos;
					keys[i].row = row;
					keys[i].playerController = playerController;
					break;
				}
				// if there is already a note playing on this channel
				// we "cut" the note
				else if (keys[i].channel == chn)
				{
					keys[i].note = keys[i].channel = 0;
				}
			}
			
			// play it
			playerLogic->playNote(*playerController, (mp_ubyte)chn, note, 
								  (mp_ubyte)ins, keyVolume);
			
			// if we're recording send the note to the pattern editor
			if (isLiveRecording)
			{
				setChanged();
			
				updateSongPosition(pos, row, true);
				pp_int32 posInner = patternEditorControl->getCursorPosInner();
				patternEditorControl->setChannel(chn, posInner);
				// add delay note if requested
				if (ticker && recordNoteDelay)
					patternEditorControl->getPatternEditor()->writeEffect(1, 0x3D, ticker > 0xf ? 0xf : ticker);
				
				if (keyVolume != -1 && keyVolume >= 0 && keyVolume <= 255)
					patternEditorControl->getPatternEditor()->writeEffect(0, 0xC, (pp_uint8)keyVolume);
				
				patternEditorControl->getPatternEditor()->writeNote(note);

				screen->paintControl(patternEditorControl);
				
				if (event)
					event->cancel();
			}
		}
		else if (event)
		{
			event->cancel();
		}
		
	}
}

void Tracker::sendNoteUpToPatternEditor(PPEvent* event, pp_int32 note, PatternEditorControl* patternEditorControl)
{
	// if this is a valid note look if we're playing something and release note by sending key-off
	if (note >= 1 && note <= ModuleEditor::MAX_NOTE)
	{	
		pp_int32 pos = -1, row = 0, ticker = 0;
		
		bool record = (editMode == EditModeMilkyTracker ? screen->hasFocus(patternEditorControl) : recordMode);	
		
		for (pp_int32 i = 0; i < 128; i++)
		{
			// found a playing channel
			if (keys[i].note == note)
			{
				PlayerController* playerController = this->playerController;
				if (keys[i].playerController)
					playerController = keys[i].playerController;
			
				bool isLiveRecording = (playerController->isPlaying() || playerController->isPlayingPattern()) && record && shouldFollowSong();
				
				ASSERT(sizeof(mp_sint32) == sizeof(pp_int32));
				
				bool recPat = false;
				if (isLiveRecording && !playerController->isPlayingPattern())
					playerController->getPosition((mp_sint32&)pos, (mp_sint32&)row, (mp_sint32&)ticker);
				else if (isLiveRecording)
				{
					playerController->getPosition((mp_sint32&)pos, (mp_sint32&)row, (mp_sint32&)ticker);
					pos = -1;
					recPat = true;
				}
							
				if (isLiveRecording && recordKeyOff)
				{									
					// send key off
					playerLogic->playNote(*playerController, (mp_ubyte)keys[i].channel, 
										  PatternTools::getNoteOffNote(), 
										  keys[i].ins);
					
					// Make sure pattern is at the current playing position
					updateSongPosition(pos, row, true);
					
					// save current cursor channel position
					pp_int32 currentChannel = patternEditorControl->getCurrentChannel();
					// save current inner cursor position
					pp_int32 posInner = patternEditorControl->getCursorPosInner();
					// select channel from the note we're playing
					patternEditorControl->setChannel(keys[i].channel, 0);
					
					// if we're in the same slot => send key off by inserting key off effect
					setChanged();
					if (keys[i].row == row && keys[i].pos == pos)
					{
						//mp_sint32 bpm, speed;
						//playerController->getSpeed(bpm, speed);
						patternEditorControl->getPatternEditor()->writeEffect(1, 0x14, ticker ? ticker : 1);
					}
					// else write key off
					else
					{
						if (ticker && recordNoteDelay)
							patternEditorControl->getPatternEditor()->writeEffect(1, 0x14, ticker);
						else
							patternEditorControl->getPatternEditor()->writeNote(PatternTools::getNoteOffNote());
					}
					// restore cursor position	
					patternEditorControl->setChannel(currentChannel, posInner);							
				
					screen->paintControl(patternEditorControl);
				}
				else
				{
					// send key off
					playerLogic->playNote(*playerController,(mp_ubyte)keys[i].channel, 
										  PatternTools::getNoteOffNote(), 
										  keys[i].ins);
				}							
				
				keys[i].note = keys[i].channel = 0;
			}
		}
		
	}
}

void Tracker::sendNoteDown(mp_sint32 note, pp_int32 volume/* = -1*/)
{
	if (volume != -1 && volume > 255)
		volume = 255;

	// Volume here is between 0 to 255, but don't forget to make the volume FT2 compatible (0..64)
	inputControlListener->sendNote(note, volume != -1 ? XModule::vol64to255((volume*64)/255) : -1);
	//sendNoteDownToPatternEditor(NULL, note, getPatternEditorControl());
}

void Tracker::sendNoteUp(mp_sint32 note)
{
	inputControlListener->sendNote(note | (1 << 16));
	//sendNoteUpToPatternEditor(NULL, note, getPatternEditorControl());
}

void Tracker::processShortcuts(PPEvent* event)
{
	if (processMessageBoxShortcuts(event))
		return;

	switch (editMode)
	{
		case EditModeMilkyTracker:
			processShortcutsMilkyTracker(event);
			break;

		case EditModeFastTracker:
			processShortcutsFastTracker(event);
			break;
			
		default:
			ASSERT(false);
	}
}

void Tracker::processShortcutsMilkyTracker(PPEvent* event)
{
	if (event->getID() == eKeyDown)
	{
		pp_uint16 keyCode = *((pp_uint16*)event->getDataPtr());
		pp_uint16 scanCode = *(((pp_uint16*)event->getDataPtr())+1);
		switch (keyCode)
		{
			case VK_F1:
			case VK_F2:
			case VK_F3:
			case VK_F4:
			case VK_F5:
			case VK_F6:
			case VK_F7:
			case VK_F8:
			case VK_F9:
			case VK_F10:
			case VK_F11:
			case VK_F12:
			{
				if (::getKeyModifier())
					goto processBindings;
					
				if (static_cast<PPControl*>(getPatternEditorControl()) != screen->getFocusedControl())
				{
					getPatternEditorControl()->callEventListener(event);
				}
				break;
			}

			default:
			{
processBindings:
				pp_int32 keyModifier = ::getKeyModifier(); 
				bool res = executeBinding(eventKeyDownBindings, keyCode);

				if (res && !isActiveEditing())
					event->cancel();
					
				if (res || keyModifier)
					break;
			
				if (editMode == EditModeMilkyTracker)
				{
					if (sectionDiskMenu->isFileBrowserVisible() &&
						sectionDiskMenu->fileBrowserHasFocus())
						break;
				}
			
				PatternEditorControl* patternEditorControl = getPatternEditorControl();

				// translate key to note
				pp_int32 note = patternEditorControl->ScanCodeToNote(scanCode);

				sendNoteDownToPatternEditor(event, note, patternEditorControl);	
				break;
			}

		}
	}
	else if (event->getID() == eKeyUp)
	{
		pp_uint16 keyCode = *((pp_uint16*)event->getDataPtr()); 
		pp_uint16 scanCode = *(((pp_uint16*)event->getDataPtr())+1);
		
		switch (keyCode)
		{
			case VK_SPACE:
			{
				playerLogic->finishTraceAndRowPlay();
				break;
			}
				
			default:
			{
				PatternEditorControl* patternEditorControl = getPatternEditorControl();
				
				pp_int32 note = patternEditorControl->ScanCodeToNote(scanCode);				
				
				sendNoteUpToPatternEditor(event, note, patternEditorControl);	
			}
		}
		
	}
}

void Tracker::selectNextOrder(bool wrap/* = false*/)
{
	if (wrap && listBoxOrderList->isLastEntry())
	{
		setOrderListIndex(0);
		return;
	}

	pp_uint16 vk[3] = {VK_DOWN, 0, 0};
	PPEvent e(eKeyDown, &vk, sizeof(vk));
	listBoxOrderList->callEventListener(&e);
}

void Tracker::selectPreviousOrder(bool wrap/* = false*/)
{
	if (wrap && listBoxOrderList->isFirstEntry())
	{
		setOrderListIndex(listBoxOrderList->getNumItems()-1);
		return;
	}

	pp_uint16 vk[3] = {VK_UP, 0, 0};
	PPEvent e(eKeyDown, &vk, sizeof(vk));
	listBoxOrderList->callEventListener(&e);
}

void Tracker::selectNextInstrument()
{
	pp_uint16 vk[3] = {VK_DOWN, 0, 0};
	PPEvent e(eKeyDown, &vk, sizeof(vk));
	listBoxInstruments->callEventListener(&e);
}

void Tracker::selectPreviousInstrument()
{
	pp_uint16 vk[3] = {VK_UP, 0, 0};
	PPEvent e(eKeyDown, &vk, sizeof(vk));
	listBoxInstruments->callEventListener(&e);
}

///////////////////////////////////////////////////////////////////////////////
// The Fasttracker II compatibility mode is really just a big hack, because
// MilkyTracker uses focus handling on most editable controls while FT2 doesn't
// It works like this:
// A few keys always go to the pattern editor
// If record mode is ON all keyboard events are also routed to pattern editor 
// (no matter if it can do something or not)
// Keys are not routed into any other control except for editing
///////////////////////////////////////////////////////////////////////////////
void Tracker::processShortcutsFastTracker(PPEvent* event)
{
	if (isActiveEditing())
		return;

	/*if (screen->getFocusedControl() != static_cast<PPControl*>(getPatternEditorControl()))
	{
		screen->setFocus(getPatternEditorControl());
		screen->paintControl(getPatternEditorControl());
	}*/

	if (event->getID() == eKeyDown)
	{
		pp_uint16 keyCode = *((pp_uint16*)event->getDataPtr());
		pp_uint16 scanCode = *(((pp_uint16*)event->getDataPtr())+1);
	
		switch (scanCode)
		{
			case SC_WTF:
				if (!::getKeyModifier() || ::getKeyModifier() == KeyModifierSHIFT)
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
					keyCode = 0;
				}
				break;

			// Place cursor in channel
			case SC_Q:
			case SC_W:
			case SC_E:
			case SC_R:
			case SC_T:
			case SC_Z:
			case SC_U:
			case SC_I:
			case SC_A:
			case SC_S:
			case SC_D:
			case SC_F:
			case SC_G:
			case SC_H:
			case SC_J:
			case SC_K:
				if (screen->getModalControl())
					break;
				
				if (::getKeyModifier() == KeyModifierALT)
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
					keyCode = 0;
				}
				break;
		}
	
		switch (keyCode)
		{
			case VK_SPACE:
			{
				if (screen->getModalControl())
					break;
					
				if (::getKeyModifier())
					goto processOthers;
			
				if (playerController->isPlaying() || playerController->isPlayingPattern())
				{
					playerLogic->stopSong();
					event->cancel();
					break;
				}
				
				playerLogic->stopSong();

				eventKeyDownBinding_ToggleFT2Edit();

				event->cancel();
				break;
			}

			// Those are the key combinations which are always routed to pattern editor control as long
			// as we're in Fasttracker editing mode
			case VK_ALT:
			case VK_SHIFT:
			case VK_CONTROL:
				if (screen->getModalControl())
					break;

				getPatternEditorControl()->callEventListener(event);
				event->cancel();
				break;
			
			// Transpose (regardless of modifers)
			case VK_F1:
			case VK_F2:
			case VK_F7:
			case VK_F8:
			case VK_F9:
			case VK_F10:
			case VK_F11:
			case VK_F12:
				processShortcutsMilkyTracker(event);
				break;

			// Cut copy paste
			case VK_F3:
			case VK_F4:
			case VK_F5:
			case VK_F6:
				// Global meaning here
				if (::getKeyModifier())
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
					break;
				}
				processShortcutsMilkyTracker(event);
				break;
				
			// Some special keys always going to the pattern editor (like undo, redo, mute etc.)
			case 'A':
			case 'C':
			case 'V':
			case 'X':
			case 'Z':
			case 'Y':
				if (screen->getModalControl())
				{
					// those seem to be piano keys, they're used in some
					// modal dialogs for instrument preview playback
					if (!::getKeyModifier())
						goto processOthers;
						
					break;
				}
				
				if (::getKeyModifier() == (KeyModifierCTRL|KeyModifierALT))
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
				}
				else goto processOthers;
				break;

			case 'I':
				if (screen->getModalControl())
					break;
				
				if (::getKeyModifier() == KeyModifierSHIFT)
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
				}
				else goto processOthers;
				break;

			case 'M':
				if (screen->getModalControl())
					break;
				
				if (::getKeyModifier() == KeyModifierSHIFT ||
					::getKeyModifier() == (KeyModifierSHIFT|KeyModifierCTRL))
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
				}
				else goto processOthers;
				break;

			case VK_UP:
			case VK_DOWN:
			case VK_LEFT:
			case VK_RIGHT:
			case VK_HOME:
			case VK_END:
			case VK_PRIOR:
			case VK_NEXT:
				if (screen->getModalControl())
					break;

				if (!::getKeyModifier() ||
					::getKeyModifier() == KeyModifierALT ||
					::getKeyModifier() == (KeyModifierSHIFT|KeyModifierALT))
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
				}
				else if (::getKeyModifier() == KeyModifierSHIFT)
				{
					switch (keyCode)
					{
						// Select instrument using Shift+Up/Down
						case VK_UP:
						case VK_DOWN:
						case VK_NEXT:
						case VK_PRIOR:
							listBoxInstruments->callEventListener(event);
							event->cancel();
							break;
						
						// Select new order using Shift+Left/Right
						case VK_LEFT:
						{
							selectPreviousOrder();
							event->cancel();
							break;
						}
						case VK_RIGHT:
						{
							selectNextOrder();
							event->cancel();
							break;
						}
					}
				}
				else if (::getKeyModifier() == (KeyModifierSHIFT|KeyModifierCTRL))
				{
					switch (keyCode)
					{
						// Select sample using Shift+Alt+Up/Down
						case VK_UP:
						case VK_DOWN:
						case VK_NEXT:
						case VK_PRIOR:
							listBoxSamples->callEventListener(event);
							event->cancel();
							break;
					}
				}
				else if (::getKeyModifier() == KeyModifierCTRL)
				{
					switch (keyCode)
					{
						// Select pattern using Ctrl+Left/Right
						case VK_LEFT:
							eventKeyDownBinding_PreviousPattern();
							event->cancel();
							break;
							
						case VK_RIGHT:
							eventKeyDownBinding_NextPattern();
							event->cancel();
							break;
					}
				}
				goto processOthers;
				break;

			case VK_TAB:
				if (screen->getModalControl())
					break;

				getPatternEditorControl()->callEventListener(event);
				event->cancel();
				break;

			default:
processOthers:
				processShortcutsMilkyTracker(event);

				if (screen->getModalControl())
					break;

				if (recordMode)
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
				}
				// if recordMode is false and focus is on pattern editor
				// we need to cancel the event in order to prevent it
				// from going into the pattern editor
				else if (screen->getFocusedControl() == static_cast<PPControl*>(getPatternEditorControl()))
				{
					event->cancel();
				}
		}
	}
	else if (event->getID() == eKeyUp)
	{
		pp_uint16 keyCode = *((pp_uint16*)event->getDataPtr());
		//pp_uint16 scanCode = *(((pp_uint16*)event->getDataPtr())+1);
	
		switch (keyCode)
		{
			// Those are the keykombinations which are always routed to pattern editor control as long
			// as we're in Fasttracker editing mode
			case VK_ALT:
			case VK_SHIFT:
			case VK_CONTROL:
				if (screen->getModalControl())
					break;

				getPatternEditorControl()->callEventListener(event);
				event->cancel();
				break;

			default:
				processShortcutsMilkyTracker(event);

				if (screen->getModalControl())
					/*break;*/return;

				if (recordMode)
				{
					getPatternEditorControl()->callEventListener(event);
					event->cancel();
				}
				// if recordMode is false and focus is on pattern editor
				// we need to cancel the event in order to prevent it
				// from going into the pattern editor
				else if (screen->getFocusedControl() == static_cast<PPControl*>(getPatternEditorControl()))
				{
					event->cancel();
				}
		}
	}
}


void Tracker::switchEditMode(EditModes mode)
{
	switch (mode)
	{
		case EditModeMilkyTracker:
		{
			PPContainer* container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_MENU));
			ASSERT(container);
			
			// Assign keyboard bindings
			eventKeyDownBindings = eventKeyDownBindingsMilkyTracker;
			
			getPatternEditorControl()->setShowFocus(true);
			listBoxInstruments->setShowFocus(true);
			listBoxSamples->setShowFocus(true);
			listBoxOrderList->setShowFocus(true);
			sectionDiskMenu->setFileBrowserShowFocus(true);
			sectionDiskMenu->setCycleFilenames(true);
			container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_ABOUT));
			ASSERT(container);
			static_cast<PPListBox*>(container->getControlByID(LISTBOX_SONGTITLE))->setShowFocus(true);
			
			screen->setFocus(listBoxInstruments, false);			
			break;
		}

		case EditModeFastTracker:
		{
			PPContainer* container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_MENU));
			ASSERT(container);

			// Assign keyboard bindings
			eventKeyDownBindings = eventKeyDownBindingsFastTracker;			

			getPatternEditorControl()->setShowFocus(false);
			listBoxInstruments->setShowFocus(false);
			listBoxSamples->setShowFocus(false);
			listBoxOrderList->setShowFocus(false);
			sectionDiskMenu->setCycleFilenames(false);
			sectionDiskMenu->setFileBrowserShowFocus(false);
			container = static_cast<PPContainer*>(screen->getControlByID(CONTAINER_ABOUT));
			ASSERT(container);
			static_cast<PPListBox*>(container->getControlByID(LISTBOX_SONGTITLE))->setShowFocus(false);
			
			recordMode = true;
			eventKeyDownBinding_ToggleFT2Edit();
			break;
		}
	}
	
	getPatternEditorControl()->switchEditMode(mode);

	editMode = mode;
}

// Process messagebox shortcuts (RETURN & ESC)
bool Tracker::processMessageBoxShortcuts(PPEvent* event)
{
	PPControl* ctrl = screen->getModalControl();
	
	if (ctrl == NULL || !ctrl->isContainer() || event->getID() != eKeyDown)
		return false;

	PPSimpleVector<PPControl>& controls = static_cast<PPContainer*>(ctrl)->getControls();

	pp_int32 i;
	for (i = 0; i < controls.size(); i++)
	{
		PPControl* ctrl = controls.get(i);
		if (ctrl->isListBox() && static_cast<PPListBox*>(ctrl)->isEditing())
			return true;
	}

	pp_uint16 keyCode = *((pp_uint16*)event->getDataPtr());

	for (i = 0; i < controls.size(); i++)
	{
		PPControl* ctrl = controls.get(i);
		switch (ctrl->getID())
		{
			case PP_MESSAGEBOX_BUTTON_YES:	
				if (keyCode == VK_RETURN)
				{
					PPPoint p = ctrl->getLocation();
					p.x+=ctrl->getSize().width >> 1;
					p.y+=ctrl->getSize().height >> 1;
					
					PPEvent e1(eLMouseDown, &p, sizeof(PPPoint));
					PPEvent e2(eLMouseUp, &p, sizeof(PPPoint));
					
					ctrl->callEventListener(&e1);
					ctrl->callEventListener(&e2);
					
					//bool res = messageBoxEventListener(screen->getModalControl()->getID(), MESSAGEBOX_BUTTON_YES);
					
					//if (res)
					//	screen->setModalControl(NULL);  // repaints
					return true;
				}
				break;
				
			case PP_MESSAGEBOX_BUTTON_CANCEL:
				if (keyCode == VK_ESCAPE)
				{
					PPPoint p = ctrl->getLocation();
					p.x+=ctrl->getSize().width >> 1;
					p.y+=ctrl->getSize().height >> 1;
					
					PPEvent e1(eLMouseDown, &p, sizeof(PPPoint));
					PPEvent e2(eLMouseUp, &p, sizeof(PPPoint));
					
					ctrl->callEventListener(&e1);
					ctrl->callEventListener(&e2);
					//bool res = messageBoxEventListener(screen->getModalControl()->getID(), MESSAGEBOX_BUTTON_CANCEL);
					
					//if (res)
					//	screen->setModalControl(NULL);  // repaints
					return true;
				}
				break;

			case PP_MESSAGEBOX_BUTTON_NO:
				if (keyCode == VK_ESCAPE)
				{
					PPPoint p = ctrl->getLocation();
					p.x+=ctrl->getSize().width >> 1;
					p.y+=ctrl->getSize().height >> 1;
					
					PPEvent e1(eLMouseDown, &p, sizeof(PPPoint));
					PPEvent e2(eLMouseUp, &p, sizeof(PPPoint));
					
					ctrl->callEventListener(&e1);
					ctrl->callEventListener(&e2);
					//bool res = messageBoxEventListener(screen->getModalControl()->getID(), MESSAGEBOX_BUTTON_NO);
					
					//if (res)
					//	screen->setModalControl(NULL);  // repaints
					return true;
				}
				break;
		}
	}
	
	return false;
}
