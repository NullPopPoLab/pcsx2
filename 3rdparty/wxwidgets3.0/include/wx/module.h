/////////////////////////////////////////////////////////////////////////////
// Name:        wx/module.h
// Purpose:     Modules handling
// Author:      Wolfram Gloger/adapted by Guilhem Lavaux
// Modified by:
// Created:     04/11/98
// Copyright:   (c) Wolfram Gloger and Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MODULE_H_
#define _WX_MODULE_H_

#include "wx/object.h"
#include "wx/list.h"
#include "wx/arrstr.h"
#include "wx/dynarray.h"

// declare a linked list of modules
class WXDLLIMPEXP_FWD_BASE wxModule;
WX_DECLARE_USER_EXPORTED_LIST(wxModule, wxModuleList, WXDLLIMPEXP_BASE);

// and an array of class info objects
WX_DEFINE_USER_EXPORTED_ARRAY_PTR(wxClassInfo *, wxArrayClassInfo,
                                    class WXDLLIMPEXP_BASE);


// declaring a class derived from wxModule will automatically create an
// instance of this class on program startup, call its OnInit() method and call
// OnExit() on program termination (but only if OnInit() succeeded)
class WXDLLIMPEXP_BASE wxModule : public wxObject
{
public:
    wxModule() {}
    virtual ~wxModule() {}

    // if module init routine returns false the application
    // will fail to startup

    bool Init() { return OnInit(); }
    void Exit() { OnExit(); }

    // Override both of these

        // called on program startup

    virtual bool OnInit() = 0;

        // called just before program termination, but only if OnInit()
        // succeeded

    virtual void OnExit() = 0;

    static void RegisterModule(wxModule *module);
    static void RegisterModules();
    static bool InitializeModules();
    static void CleanUpModules() { DoCleanUpModules(m_modules); }

    // used by wxObjectLoader when unloading shared libs's

    static void UnregisterModule(wxModule *module);

protected:
    static wxModuleList m_modules;

private:
    // initialize module and Append it to initializedModules list recursively
    // calling itself to satisfy module dependencies if needed
    static bool
    DoInitializeModule(wxModule *module, wxModuleList &initializedModules);

    // cleanup the modules in the specified list (which may not contain all
    // modules if we're called during initialization because not all modules
    // could be initialized) and also empty m_modules itself
    static void DoCleanUpModules(const wxModuleList& modules);

    // module dependencies: contains wxClassInfo pointers for all modules which
    // must be initialized before this one
    wxArrayClassInfo m_dependencies;

    // used internally while initializing/cleaning up modules
    enum
    {
        State_Registered,   // module registered but not initialized yet
        State_Initializing, // we're initializing this module but not done yet
        State_Initialized   // module initialized successfully
    } m_state;


    DECLARE_CLASS(wxModule)
};

#endif // _WX_MODULE_H_
