#include "cbase.h"

#if defined( GAME_DLL )

class CGashuntPersistentVar : public CPointEntity
{
    DECLARE_CLASS(CGashuntPersistentVar, CPointEntity);
    DECLARE_DATADESC();

public:
    void InputSetValue(inputdata_t& inputdata) { m_iHealth = inputdata.value.Int(); }
    void InputAddValue(inputdata_t& inputdata) { m_iHealth += inputdata.value.Int(); }

    COutputInt m_OnRestored;
};

BEGIN_DATADESC(CGashuntPersistentVar)
// We repurpose m_iHealth for native auto-save support, avoiding complex custom fields
DEFINE_INPUTFUNC(FIELD_INTEGER, "SetValue", InputSetValue),
DEFINE_INPUTFUNC(FIELD_INTEGER, "AddValue", InputAddValue),
DEFINE_OUTPUT(m_OnRestored, "OnRestored"),
END_DATADESC()

LINK_ENTITY_TO_CLASS(gashunt_persistentdata, CGashuntPersistentVar);

#endif // GAME_DLL