/*
  Hotkeys.cpp: hotkeys (keyboard shortcuts)
  Copyright 2013 by Vincent Fourmond

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <GG/DrawUtil.h>
#include <GG/MultiEdit.h>
#include <GG/WndEvent.h>
#include <GG/Layout.h>
#include <GG/GUI.h>

#include <algorithm>
#include <vector>
#include <set>

#include "Hotkeys.h"

#include "../util/OptionsDB.h"
#include "../util/i18n.h"
#include "../util/Logger.h"

#include <sstream>

/// A helper class that stores both a connection and the
/// conditions in which it should be on.
struct HotkeyManager::ConditionalConnection {
    /// Block or unblocks the connection based on condition.
    void UpdateConnection() {
        if (connection.connected()) {
            if (!condition || condition->IsActive())
                blocker.unblock();
            else
                blocker.block();
        }
    };

    ConditionalConnection(const boost::signals2::connection& conn,
                            HotkeyCondition* cond) :
        condition(cond), connection(conn), blocker(connection)
    {
        blocker.unblock();
    }

    /// The condition. If null, always on.
    boost::shared_ptr<HotkeyCondition> condition;

    boost::signals2::connection connection;
    boost::signals2::shared_connection_block blocker;
};

/////////////////////////////////////////////////////////
// Hotkey
/////////////////////////////////////////////////////////
std::map<std::string, Hotkey>* Hotkey::s_hotkeys = 0;

void Hotkey::AddHotkey(const std::string& name, const std::string& description, GG::Key key, GG::Flags<GG::ModKey> mod) {
    if (!s_hotkeys)
        s_hotkeys = new std::map<std::string, Hotkey>;
    if (IsTypingSafe(key, mod)) {
        s_hotkeys->insert(std::make_pair(name, Hotkey(name, description, key, mod)));
    } else {
        ErrorLogger() << "Hotkey::AddHotkey attempted to set a hotkey that is not safe to use while typing: "
                      << "name: " << name << " key/mod: " << key << " / " << mod;
        s_hotkeys->insert(std::make_pair(name, Hotkey(name, description, GG::GGK_NONE, GG::MOD_KEY_NONE)));
    }
}

std::string Hotkey::HotkeyToString(GG::Key key, GG::Flags<GG::ModKey> mod) {
    std::ostringstream s;
    if (mod != GG::MOD_KEY_NONE) {
        s << mod;
        s << "+";
    }
    if (key > GG::GGK_UNKNOWN) {
        s << key;
    }
    return s.str();
}

std::set<std::string> Hotkey::DefinedHotkeys() {
    std::set<std::string> retval;
    if (s_hotkeys) {
        for (std::map<std::string, Hotkey>::iterator i = s_hotkeys->begin();
             i != s_hotkeys->end(); i++)
        { retval.insert(i->first); }
    }
    return retval;
}

std::string Hotkey::ToString() const
{ return HotkeyToString(m_key, m_mod_keys); }

std::pair<GG::Key, GG::Flags<GG::ModKey> > Hotkey::HotkeyFromString(const std::string& str) {
    if (str.empty())
        return std::make_pair(GG::GGK_NONE, GG::Flags<GG::ModKey>());

    size_t plus = str.find_first_of("+");
    std::string v = str;
    GG::Flags<GG::ModKey> mod = GG::MOD_KEY_NONE;
    if (plus != std::string::npos) {
        // We have a modifier. Things get a little complex, since we need
        // to handle the |-separated flags:
        std::string m = str.substr(0, plus);
        v = str.substr(plus);

        size_t found = 0;
        size_t prev = 0;
        while (true) {
            found = m.find(" | ", prev);
            std::string sub = m.substr(prev, found);
            GG::ModKey cm = GG::FlagSpec<GG::ModKey>::instance().FromString(sub);
            mod |= cm;
            if (found == std::string::npos)
                break;
            prev = found + 3;
        }
        v = str.substr(plus+1);
    }
    std::istringstream s(v);
    GG::Key key;
    s >> key;
    return std::pair<GG::Key, GG::Flags<GG::ModKey> >(key, mod);
}

void Hotkey::SetFromString(const std::string& str) {
    std::pair<GG::Key, GG::Flags<GG::ModKey> > km = HotkeyFromString(str);
    m_key = km.first;
    m_mod_keys = km.second;
}

void Hotkey::AddOptions(OptionsDB& db) {
    if (!s_hotkeys)
        return;
    for (std::map<std::string, Hotkey>::const_iterator i = s_hotkeys->begin();
         i != s_hotkeys->end(); i++)
    {
        const Hotkey& hotkey = i->second;
        std::string n = "UI.hotkeys.";
        n += hotkey.m_name;
        db.Add(n, hotkey.GetDescription(),
               hotkey.ToString(), Validator<std::string>());
    }
}

static void ReplaceInString(std::string& str, const std::string& what,
                            const std::string& replacement)
{
    size_t lst = 0;
    size_t l1 = what.size();
    size_t l2 = replacement.size();
    if (l1 == 0 && l2 == 0)
        return;                 // Nothing to do
    do {
        size_t t = str.find(what, lst);
        if(t == std::string::npos)
            return;
        str.replace(t, l1, replacement);
        t += l2;
    } while(true);
}

std::string Hotkey::PrettyPrint(GG::Key key, GG::Flags<GG::ModKey> mod) {
    std::string retval;
    if (mod & GG::MOD_KEY_CTRL)
        retval += "CTRL+";
    if (mod & GG::MOD_KEY_ALT)
        retval += "ALT+";
    if (mod & GG::MOD_KEY_SHIFT)
        retval += "SHIFT+";
    if (mod & GG::MOD_KEY_META)
        retval += "META+";

    std::ostringstream key_stream;
    key_stream << key;
    std::string key_string = key_stream.str();
    ReplaceInString(key_string, "GGK_", "");

    retval += key_string;
    return retval;
}

std::string Hotkey::PrettyPrint() const
{ return PrettyPrint(m_key, m_mod_keys); }

void Hotkey::ReadFromOptions(OptionsDB& db) {
    for (std::map<std::string, Hotkey>::iterator i = s_hotkeys->begin();
         i != s_hotkeys->end(); i++)
    {
        Hotkey& hotkey = i->second;

        std::string options_db_name = "UI.hotkeys." + hotkey.m_name;
        if (!db.OptionExists(options_db_name)) {
            ErrorLogger() << "Hotkey::ReadFromOptions : no option for " << options_db_name;
            continue;
        }
        std::string option_string = db.Get<std::string>(options_db_name);
        std::pair<GG::Key, GG::Flags<GG::ModKey> > key_modkey_pair = HotkeyFromString(option_string);

        if (key_modkey_pair.first == GG::GGK_NONE)
            continue;

        if (key_modkey_pair.first == GG::EnumMap<GG::Key>::BAD_VALUE) {
            ErrorLogger() << "Hotkey::ReadFromOptions : Invalid key spec: '"
                                   << option_string << "' for hotkey " << hotkey.m_name;
            continue;
        }

        if (!IsTypingSafe(key_modkey_pair.first, key_modkey_pair.second)) {
            ErrorLogger() << "Hotkey::ReadFromOptions : Typing-unsafe key spec: '"
                                   << option_string << "' for hotkey " << hotkey.m_name;
            continue;
        }

        hotkey.m_key = key_modkey_pair.first;
        hotkey.m_mod_keys = key_modkey_pair.second;
    }
}

Hotkey::Hotkey(const std::string& name, const std::string& description, GG::Key key, GG::Flags<GG::ModKey> mod) :
    m_name(name),
    m_description(description),
    m_key(key),
    m_key_default(key),
    m_mod_keys(mod),
    m_mod_keys_default(mod)
{}

const Hotkey& Hotkey::NamedHotkey(const std::string& name)
{ return PrivateNamedHotkey(name); }

std::string Hotkey::GetDescription() const
{ return m_description; }

Hotkey& Hotkey::PrivateNamedHotkey(const std::string& name) {
    std::string error_msg = "Hotkey::PrivateNamedHotkey error: no hotkey named: " + name;

    if (!s_hotkeys)
        throw std::runtime_error("Hotkey::PrivateNamedHotkey error: couldn't get hotkeys container.");

    std::map<std::string, Hotkey>::iterator i = s_hotkeys->find(name);
    if (i == s_hotkeys->end())
        throw std::invalid_argument(error_msg.c_str());

    return i->second;
}

std::map<std::string, std::set<std::string> > Hotkey::ClassifyHotkeys() {
    std::map<std::string, std::set<std::string> > ret;
    if (s_hotkeys) {
        for (std::map<std::string, Hotkey>::iterator i = s_hotkeys->begin();
             i != s_hotkeys->end(); i++)
        {
            const std::string& hk_name = i->first;
            std::string section = "HOTKEYS_GENERAL";
            size_t j = hk_name.find('.');
            if (j != std::string::npos) {
                section = "HOTKEYS_" + hk_name.substr(0, j);
                std::transform(section.begin(), section.end(), 
                               section.begin(), ::toupper);
                if (section == "HOTKEYS_COMBAT")
                    section = "HOTKEYS_Z_COMBAT"; // make combat the
                                                  // last category
            }
            ret[section].insert(hk_name);
        }
    }
    return ret;
}

bool Hotkey::IsTypingSafe(GG::Key key, GG::Flags<GG::ModKey> mod) {
    if (mod & (GG::MOD_KEY_CTRL | GG::MOD_KEY_ALT | GG::MOD_KEY_META))
        return true;
    if (key >= GG::GGK_F1 && key <= GG::GGK_F15)
        return true;
    if (key == GG::GGK_TAB || key == GG::GGK_ESCAPE || key == GG::GGK_NONE)
        return true;
    return false;
}

bool Hotkey::IsTypingSafe() const
{ return IsTypingSafe(m_key, m_mod_keys); }

bool Hotkey::IsDefault() const
{ return m_key == m_key_default && m_mod_keys == m_mod_keys_default; }

void Hotkey::SetHotkey(const Hotkey& hotkey, GG::Key key, GG::Flags<GG::ModKey> mod) {
    if (!IsTypingSafe(key, mod)) {
        DebugLogger() << "Hotkey::SetHotkey: Typing-unsafe hotkey requested: "
                               << mod << " + " << key << " for hotkey " << hotkey.m_name;
        return;
    }

    Hotkey& hk = PrivateNamedHotkey(hotkey.m_name);
    hk.m_key = key;
    hk.m_mod_keys = GG::MassagedAccelModKeys(mod);

    GetOptionsDB().Set<std::string>("UI.hotkeys." + hk.m_name, hk.ToString());
}

void Hotkey::ResetHotkey(const Hotkey& old_hotkey) {
    Hotkey& hk = PrivateNamedHotkey(old_hotkey.m_name);
    hk.m_key = hk.m_key_default;
    hk.m_mod_keys = hk.m_mod_keys_default;
    GetOptionsDB().Set<std::string>("UI.hotkeys." + hk.m_name, hk.ToString());
}

void Hotkey::ClearHotkey(const Hotkey& old_hotkey)
{ Hotkey::SetHotkey(old_hotkey, GG::GGK_NONE, GG::Flags<GG::ModKey>()); }

//////////////////////////////////////////////////////////////////////
// InvisibleWindowCondition
//////////////////////////////////////////////////////////////////////
InvisibleWindowCondition::InvisibleWindowCondition(GG::Wnd* w1, GG::Wnd* w2,
                                                   GG::Wnd* w3, GG::Wnd* w4)
{
    m_blacklist.push_back(w1);
    if (w2)
        m_blacklist.push_back(w2);
    if (w3)
        m_blacklist.push_back(w3);
    if (w4)
        m_blacklist.push_back(w4);
}

InvisibleWindowCondition::InvisibleWindowCondition(const std::list<GG::Wnd*>& bl) :
    m_blacklist(bl)
{}

bool InvisibleWindowCondition::IsActive() const {
    for (std::list<GG::Wnd*>::const_iterator i = m_blacklist.begin();
         i != m_blacklist.end(); i++)
    {
        if ((*i)->Visible())
            return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////
// OrCondition
//////////////////////////////////////////////////////////////////////
OrCondition::OrCondition(HotkeyCondition* c1, HotkeyCondition* c2,
                         HotkeyCondition* c3, HotkeyCondition* c4,
                         HotkeyCondition* c5, HotkeyCondition* c6,
                         HotkeyCondition* c7, HotkeyCondition* c8)
{
    m_conditions.push_back(c1);
    m_conditions.push_back(c2);
    if (c3)
        m_conditions.push_back(c3);
    if (c4)
        m_conditions.push_back(c4);
    if (c5)
        m_conditions.push_back(c5);
    if (c6)
        m_conditions.push_back(c6);
    if (c7)
        m_conditions.push_back(c7);
    if (c8)
        m_conditions.push_back(c8);
}

bool OrCondition::IsActive() const {
    for (std::list<HotkeyCondition*>::const_iterator i = m_conditions.begin();
         i != m_conditions.end(); i++)
    {
        if ((*i)->IsActive())
            return true;
    }
    return false;
}

OrCondition::~OrCondition() {
    for (std::list<HotkeyCondition*>::iterator i = m_conditions.begin();
         i != m_conditions.end(); i++)
    { delete *i; }
}

//////////////////////////////////////////////////////////////////////
// HotkeyManager
//////////////////////////////////////////////////////////////////////
HotkeyManager* HotkeyManager::s_singleton = 0;

HotkeyManager::HotkeyManager() :
    m_hotkey_disablings_count(0)
{}

HotkeyManager::~HotkeyManager()
{}

HotkeyManager* HotkeyManager::GetManager() {
    if (!s_singleton)
        s_singleton = new HotkeyManager;
    return s_singleton;
}

void HotkeyManager::RebuildShortcuts() {
    for (std::set<boost::signals2::connection>::iterator i = m_internal_connections.begin();
         i != m_internal_connections.end(); i++)
    { i->disconnect(); }
    m_internal_connections.clear();

    /// @todo Disable the shortcuts that we've enabled so far ? Is it
    /// really necessary ? An unconnected signal should simply be
    /// ignored.

    // Now, build up again all the shortcuts
    GG::GUI* gui = GG::GUI::GetGUI();
    for (Connections::iterator i = m_connections.begin(); i != m_connections.end(); i++) {
        const Hotkey& hk = Hotkey::NamedHotkey(i->first);

        gui->SetAccelerator(hk.m_key, hk.m_mod_keys);

        m_internal_connections.insert(GG::Connect(
            gui->AcceleratorSignal(hk.m_key, hk.m_mod_keys),
            boost::bind(&HotkeyManager::ProcessNamedShortcut,
            this, hk.m_name)));
    }
}

void HotkeyManager::AddConditionalConnection(const std::string& name,
                                             const boost::signals2::connection& conn,
                                             HotkeyCondition* cond)
{
    ConditionalConnectionList& list = m_connections[name];
    list.push_back(ConditionalConnection(conn, cond));
}

GG::GUI::AcceleratorSignalType& HotkeyManager::NamedSignal(const std::string& name) {
    /// Unsure why GG::AcceleratorSignal implementation uses shared
    /// pointers. Maybe I should, too ?
    GG::GUI::AcceleratorSignalType*& sig = m_signals[name];
    if (!sig)
        sig = new GG::GUI::AcceleratorSignalType;
    return *sig;
}

bool HotkeyManager::ProcessNamedShortcut(const std::string& name) {
    // First update the connection state according to the current status.
    ConditionalConnectionList& conds = m_connections[name];
    for (ConditionalConnectionList::iterator i = conds.begin();
         i != conds.end(); i++)
    {
        i->UpdateConnection();
        if (!i->connection.connected())
            i = conds.erase(i);
    }

    // Then, return the value of the signal !
    GG::GUI::AcceleratorSignalType* sig = m_signals[name];
    return (*sig)();
}
