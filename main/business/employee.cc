/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
 *   This program is free software: you can redistribute it and/or modify 
 *   it under the terms of the GNU General Public License as published by 
 *   the Free Software Foundation, either version 3 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful, 
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * employee.cc - revision 174 (10/13/98)
 * Implementation of employee module
 */

#include "employee.hh"
#include "data_file.hh"
#include "report.hh"
#include "settings.hh"
#include "labor.hh"
#include "labels.hh"
#include "system.hh"
#include "safe_string_utils.hh"
#include <cctype>
#include <cstring>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Definitions ****/
#define BASE_KEY 10
#define BASE_ID  10


/**** Global Data ****/
const char* JobName[] = {
    "No Job", "Dishwasher", "Busperson", "Line Cook", "Prep Cook", "Chef",
    "Cashier", "Server", "Server/Cashier", "Bartender", "Host/Hostess",
    "Bookkeeper", "Supervisor", "Assistant Manager", "Manager", nullptr};
int JobValue[] = {
    JOB_NONE, JOB_DISHWASHER, JOB_BUSPERSON, JOB_COOK, JOB_COOK2, JOB_COOK3,
    JOB_CASHIER, JOB_SERVER, JOB_SERVER2, JOB_BARTENDER, JOB_HOST,
    JOB_BOOKKEEPER, JOB_MANAGER, JOB_MANAGER2, JOB_MANAGER3, -1};

const char* PayRateName[] = {
    "Hour", "Day", "Week", "Month", nullptr};
int PayRateValue[] = {
    PERIOD_HOUR, PERIOD_DAY, PERIOD_WEEK, PERIOD_MONTH, -1};


/**** Functions ****/
int FixPhoneNumber(Str &phone)
{
    genericChar str[32];
    const genericChar* p = phone.Value();
    genericChar *s = str;
    while (*p)
    {
        if (isdigit(*p) || *p == ' ')
            *s++ = *p;
        ++p;
    }
    *s = '\0';
    genericChar tmp[32];
    if (strlen(str) == 7)
    {
        vt_safe_string::safe_format(tmp, 32, "   %s", str);
        phone.Set(tmp);
    }
    else
        phone.Set(str);
    return 0;
}

const char* FormatPhoneNumber(Str &phone)
{
    static genericChar buffer[32];
    if (phone.size() < 10)
        vt_safe_string::safe_copy(buffer, 32, "---");
    else
    {
        const genericChar* p = phone.Value();
        genericChar str[16];
        vt_safe_string::safe_format(str, 16, "%c%c%c-%c%c%c%c", p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
        if (p[0] != ' ')
            vt_safe_string::safe_format(buffer, 32, "(%c%c%c) %s", p[0], p[1], p[2], str);
        else
            vt_safe_string::safe_copy(buffer, 32, str);
    }
    return buffer;
}

int FixSSN(Str &ssn)
{
    genericChar str[32];
    const genericChar* n = ssn.Value();
    genericChar *s = str;
    while (*n)
    {
        if (isdigit(*n))
            *s++ = *n;
        ++n;
    }
    *s = '\0';
    ssn.Set(str);
    return 0;
}

static int UserNameCompare(const void *u1, const void *u2)
{
    Employee *e1 = *(Employee **)u1;
    Employee *e2 = *(Employee **)u2;
    int val = StringCompare(e1->last_name.Value(), e2->last_name.Value());
    if (val)
        return val;

    val = StringCompare(e1->first_name.Value(), e2->first_name.Value());
    if (val)
        return val;
    else
        return StringCompare(e1->system_name.Value(), e2->system_name.Value());
}

static int UserIdCompare(const void *u1, const void *u2)
{
    int id1 = (*(Employee **)u1)->id;
    int id2 = (*(Employee **)u2)->id;
    if (id1 < id2)
        return -1;
    else if (id1 > id2)
        return 1;
    else
        return 0;
}

/**** JobInfo Class ****/
// Constructor
JobInfo::JobInfo()
{
    FnTrace("JobInfo::JobInfo()");
    next = nullptr;
    fore = nullptr;
    job = 0;
    starting_page = -1;
    curr_starting_page = -1;
    pay_rate   = PERIOD_HOUR;
    pay_amount = 0;
    dept_code = 0;
}

// Member Functions
int JobInfo::Read(InputDataFile &df, int version)
{
    FnTrace("JobInfo::Read()");
    df.Read(job);
    df.Read(pay_rate);
    df.Read(pay_amount);
    df.Read(starting_page);
    curr_starting_page = starting_page;
    if (version >= 8)
        df.Read(dept_code);
    return 0;
}

int JobInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("JobInfo::Write()");
    int error = 0;
    error += df.Write(job);
    error += df.Write(pay_rate);
    error += df.Write(pay_amount);
    error += df.Write(starting_page, 1);
    if (version >= 8)
        error += df.Write(dept_code);
    return error;
}

const char* JobInfo::Title(Terminal *t)
{
    FnTrace("JobInfo::Title()");
    const genericChar* s = FindStringByValue(job, JobValue, JobName, UnknownStr);
    return t->Translate(s);
}

/**** UserDB Class ****/
// Constructor
UserDB::UserDB()
{
    FnTrace("UserDB::UserDB()");
    changed = 0;

    super_user = new Employee;
    if (super_user)
    {
        auto *j = new JobInfo;
        j->job = JOB_SUPERUSER;
        super_user->Add(j);
        super_user->system_name.Set("Super User");
        super_user->id       = 1;
        super_user->key      = SUPERUSER_KEY;
        super_user->training = 1;
    }

    developer = new Employee;
    if (developer)
    {
        auto *j = new JobInfo;
        j->job = JOB_DEVELOPER;
        developer->Add(j);
        developer->system_name.Set("Editor");
        developer->id       = 2;
        developer->training = 1;
    }

    name_array = nullptr;
    id_array = nullptr;
}

// Destructor
UserDB::~UserDB()
{
    FnTrace("UserDB::~UserDB()");
    Purge();
    if (super_user) delete super_user;
    if (developer)  delete developer;
}

// Member Functions
int UserDB::Load(const char* file)
{
    FnTrace("UserDB::Load()");
    if (file)
        filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(filename.Value(), version))
        return 1;

    if (version < 7 || version > 8)
    {
        genericChar str[64];
        vt_safe_string::safe_format(str, 64, "Unknown UserDB file version %d", version);
        ReportError(str);
        return 1;
    }

    int n = 0;
    df.Read(n);
    for (int i = 0; i < n; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of UserDB file");
            return 1;
        }

        auto *e = new Employee;
        if (e == nullptr)
        {
            ReportError("Couldn't create employee record");
            return 1;
        }
        if (e->Read(df, version))
        {
            ReportError("Error reading employee record");
            return 1;
        }
        Add(e);
    }
    return 0;
}

int UserDB::Save()
{
    FnTrace("UserDB::Save()");
    if (filename.empty())
        return 1;

    BackupFile(filename.Value());

    // Save version 8
    OutputDataFile df;
    if (df.Open(filename.Value(), 8, 1))
        return 1;

    int error = 0;
    error += df.Write(UserCount(), 1);
    Employee *e = UserList();
    while (e)
    {
        error += e->Write(df, 8);
        e = e->next;
    }
    changed = 0;
    return error;
}

int UserDB::Add(Employee *e)
{

    FnTrace("UserDB::Add(Employee)");
    if (e == nullptr)
        return 1;

    if (name_array)
    {
	free(name_array);
        name_array = nullptr;
    }
    if (id_array)
    {
	free(id_array);
        id_array = nullptr;
    }

    if (e->id <= 0)
        e->id  = FindUniqueID();
    if (e->key <= 0)
        e->key = FindUniqueKey();

    return user_list.AddToTail(e);
}

int UserDB::Remove(Employee *e)
{

    FnTrace("UserDB::Remove(Employee)");
    if (e == nullptr)
        return 1;

    if (name_array)
    {
	free(name_array);
        name_array = nullptr;
    }
    if (id_array)
    {
	free(id_array);
        id_array = nullptr;
    }

    return user_list.Remove(e);
}

int UserDB::Purge()
{

    FnTrace("UserDB::Purge()");
    if (name_array)
    {
	free(name_array);
        name_array = nullptr;
    }
    if (id_array)
    {
	free(id_array);
        id_array = nullptr;
    }

    user_list.Purge();
    return 0;
}

int UserDB::Init(LaborDB *db)
{
    FnTrace("UserDB::Init()");
    for (Employee *e = UserList(); e != nullptr; e = e->next)
        e->last_job = db->CurrentJob(e);
    return 0;
}

Employee *UserDB::FindByID(int user_id)
{
    FnTrace("UserDB::FindByID()");
    for (Employee *e = UserList(); e != nullptr; e = e->next)
    {
        if (e->id == user_id)
            return e;
    }

    if (developer && developer->id == user_id)
        return developer;
    else if (super_user && super_user->id == user_id)
        return super_user;

    return nullptr;
}

Employee *UserDB::FindByKey(int key)
{
    FnTrace("UserDB::FindByKey()");
    if (developer && key == developer->key)
        return developer;

    for (Employee *e = UserList(); e != nullptr; e = e->next)
        if (e->key == key)
            return e;

    if (super_user && super_user->key == key)
        return super_user;

    return nullptr;
}

Employee *UserDB::FindByName(const char* name)
{
    FnTrace("UserDB::FindByName()");
    for (Employee *e = UserList(); e != nullptr; e = e->next)
        if (StringCompare(e->system_name.Value(), name) == 0)
            return e;
    return nullptr;
}

Employee *UserDB::NameSearch(const std::string &name, Employee *user)
{
    FnTrace("UserDB::NameSearch()");
    if (name.empty())
        return nullptr;

    if (user)
        for (Employee *e = user->next; e != nullptr; e = e->next)
            if (StringCompare(e->system_name.Value(), name, static_cast<int>(name.size())) == 0)
                return e;

    for (Employee *e = UserList(); e != nullptr; e = e->next)
        if (StringCompare(e->system_name.Value(), name, static_cast<int>(name.size())) == 0)
            return e;
    return nullptr;
}

int UserDB::FindRecordByWord(Terminal *t, const std::string &word, int active, int start)
{
    FnTrace("UserDB::FindRecordByWord()");
    int val = -1;
    try {
        val = std::stoi(word);
    } catch (const std::exception&) {
        // If conversion fails, val remains -1
    }
    int len = static_cast<int>(word.size());

    Employee **array = NameArray();
    if (array == nullptr)
        return -1;
    
    int record = 0, loop = 0;
    int idx = 0;
    while (loop < 2)
    {
        // Critical fix: Check array bounds before accessing
        if (idx >= UserCount())
        {
            ++loop;
            record = 0;
            idx  = 0;
            start  = -1;
            continue;
        }
        
        Employee *e = array[idx];
        if (active < 0 || e->active == active)
        {
            if (record > start)
            {
                // check for matching key
                if (val >= 0 && e->key == val)
                    return record;
                // check for system name
                if (StringCompare(e->system_name.Value(), word, len) == 0)
                    return record;
                // check for last name
                if (StringCompare(e->last_name.Value(), word, len) == 0)
                    return record;
                // check for first name
                if (StringCompare(e->first_name.Value(), word, len) == 0)
                    return record;
                // check for address
                if (StringCompare(e->address.Value(), word, len) == 0)
                    return record;
                // check for ssn
                if (StringCompare(e->ssn.Value(), word, len) == 0)
                    return record;
            }
            ++record;
        }
        ++idx;
    }

    // search failed
    return -1;
}

Employee *UserDB::FindByRecord(Terminal *t, int record, int active)
{
    FnTrace("UserDB::FindByRecord()");
    if (record < 0)
        return nullptr;

    Employee **array = NameArray();
    if (array == nullptr)
        return nullptr;
        
    for (int i = 0; i < UserCount(); ++i)
    {
        Employee *e = array[i];
        if (e->Show(t, active))
        {
            --record;
            if (record < 0)
                return e;
        }
    }
    return nullptr;
}

int UserDB::FindUniqueID()
{
    FnTrace("UserDB::FindUniqueID()");
    int new_id = BASE_ID;
    for (;;)
    {
        Employee *e = FindByID(new_id);
        if (e == nullptr)
            return new_id;
        ++new_id;
    }
}

int UserDB::FindUniqueKey()
{
    FnTrace("UserDB::FindUniqueKey()");
    int new_key = BASE_KEY;
    for (;;)
    {
        Employee *e = FindByKey(new_key);
        if (e == nullptr)
            return new_key;
        ++new_key;
    }
}

int UserDB::ListReport(Terminal *t, int active, Report *r)
{
    FnTrace("UserDB::ListReport()");
    if (r == nullptr)
        return 1;
    LaborDB *ldb = &(t->system_data->labor_db);

    r->update_flag = UPDATE_USERS;
    r->min_width = 50;
    r->max_width = 80;
    Employee **array = NameArray(1);
    genericChar str[256], str2[256];
    int count = 0;
    for (int i = 0; i < UserCount(); ++i)
    {
        Employee *e = array[i];
        if (e->Show(t, active))
        {
            int col = COLOR_DEFAULT;
            if (e->last_job > 0)
                col = COLOR_DK_BLUE;
                
            if (1 == ldb->IsUserOnBreak(e))
                col = COLOR_DK_GREEN;

            Employee *conflict = KeyConflict(e);
            if (conflict)
                col = COLOR_DK_RED;

            r->TextC(e->JobTitle(t), col);

            if (e->last_name.size() > 0)
                vt_safe_string::safe_format(str, 256, "%s, %s", e->last_name.Value(), e->first_name.Value());
            else if (e->system_name.size() > 0)
                vt_safe_string::safe_copy(str, 256, e->system_name.Value());
            else
                vt_safe_string::safe_copy(str, 256, "---");

            if (conflict)
            {
                vt_safe_string::safe_format(str2, 256, " (ID Conflict with %s)", conflict->system_name.Value());
                vt_safe_string::safe_concat(str, 256, str2);
            }

            r->TextL(str, col);
            r->TextR(FormatPhoneNumber(e->phone), col);
            r->NewLine();
            ++count;
        }
    }

    if (count == 0)
    {
        if (active)
            r->TextC("There Are No Active Employees");
        else
            r->TextC("There Are No Inactive Employees");
    }
    return 0;
}

int UserDB::UserCount(Terminal *t, int active)
{
    FnTrace("UserDB::UserCount()");
    int count = 0;
    for (Employee *e = UserList(); e != nullptr; e = e->next)
        if (e->Show(t, active))
            ++count;
    return count;
}

Employee *UserDB::NextUser(Terminal *term, Employee *employee, int active)
{
    FnTrace("UserDB::NextUser()");
    if (employee == nullptr || UserList() == nullptr)
        return nullptr;

    if (employee == super_user || employee == developer)
        return NextUser(term, UserListEnd(), active);

    Settings *s = term->GetSettings();
    int count = 0;
    Employee *em = employee->next;
    while (em != employee)
    {
        if (em == nullptr)
        {
            em = UserList();
            ++count;
            if (count > 2)
                return nullptr;
        }
        if ((em->active == active || active < 0) && em->CanEnterSystem(s))
            return em;
        em = em->next;
    }

    return nullptr;
}

Employee *UserDB::ForeUser(Terminal *t, Employee *e, int active)
{
    FnTrace("UserDB::ForeUser()");
    if (e == nullptr || UserListEnd() == nullptr)
        return nullptr;

    if (e == super_user || e == developer)
        return ForeUser(t, UserList(), active);

    Settings *s = t->GetSettings();
    int count = 0;
    Employee *em = e->fore;
    while (em != e)
    {
        if (em == nullptr)
        {
            em = UserListEnd();
            ++count;
            if (count > 2)
                return nullptr;
        }
        if ((em->active == active || active < 0) && em->CanEnterSystem(s))
            return em;
        em = em->fore;
    }
    return nullptr;
}

int UserDB::ChangePageID(int old_id, int new_id)
{
    FnTrace("UserDB::ChangePageID()");
    if (old_id <= 0)
        return 0;  // no changes

    int changes = 0;
    for (Employee *e = UserList(); e != nullptr; e = e->next)
        for (JobInfo *j = e->JobList(); j != nullptr; j = j->next)
            if (j->starting_page == old_id)
            {
                ++changes;
                j->starting_page = new_id;
            }

    if (changes > 0)
        changed = 1;
    return changes;
}

Employee *UserDB::NewUser()
{
    FnTrace("UserDB::NewUser()");
    Employee *e = UserList(), *enext;
    while (e)
    {
        enext = e->next;
        if (e->IsBlank())
        {
            Remove(e);
            delete e;
        }
        e = enext;
    }
    e = new Employee;
    if (e)
    {
        auto *j = new JobInfo;
        if (j)
        {
            e->Add(j);
            Add(e);
        }
        else
        {
            // Memory allocation failed for JobInfo
            delete e;
            e = nullptr;
            fprintf(stderr, "ERROR: Failed to allocate JobInfo in NewUser()\n");
        }
    }
    else
    {
        // Memory allocation failed for Employee
        fprintf(stderr, "ERROR: Failed to allocate Employee in NewUser()\n");
    }
    return e;
}

Employee *UserDB::KeyConflict(Employee *server)
{
    FnTrace("UserDB::KeyConflict()");
    for (Employee *e = UserList(); e != nullptr; e = e->next)
        if (e != server && e->key == server->key)
            return e; // key conflict
    return nullptr;  // no conflicts
}

Employee **UserDB::NameArray(int resort)
{
    FnTrace("UserDB::NameArray()");
    int users = UserCount();
    if (name_array == nullptr)
    {
        resort = 1;
	name_array = (Employee **)calloc(sizeof(Employee *), users);
    }

    if (resort)
    {
        int i = 0;
        Employee *e = UserList();
        while (e)
        {
            name_array[i++] = e;
            e = e->next;
        }
        qsort(name_array, users, sizeof(Employee *), UserNameCompare);
    }
    return name_array;
}

Employee **UserDB::IdArray(int resort)
{
    FnTrace("UserDB::IdArray()");
    int users = UserCount();
    if (id_array == nullptr)
    {
        resort = 1;
	id_array = (Employee **)calloc(sizeof(Employee *), users);
    }

    if (resort)
    {
        int i = 0;
        for (Employee *e = UserList(); e != nullptr; e = e->next)
            id_array[i++] = e;

        qsort(id_array, users, sizeof(Employee *), UserIdCompare);
    }
    return id_array;
}


/**** Employee Class ****/
// Constructor
Employee::Employee()
{
    FnTrace("Employee::Employee()");
    next           = nullptr;
    fore           = nullptr;
    id             = 0;
    employee_no    = 0;
    training       = 1;  // new employee default to training mode
    key            = 0;
    access_code    = 0;
    drawer         = 0;
    security_flags = 0;
    active         = 1;
    current_job    = 0;
    last_job       = 0;
}

// Member Functions
int Employee::Read(InputDataFile &df, int version)
{
    FnTrace("Employee::Read()");
    // VERSION NOTES
    // 7 (2/26/97) earliest supported version
    // 8 (8/13/97) dept code for each job; forced case convention

    df.Read(system_name);
    system_name = AdjustCase(system_name.c_str());
    df.Read(last_name);
    last_name = AdjustCase(last_name.c_str());
    df.Read(first_name);
    first_name = AdjustCase(first_name.c_str());
    df.Read(address);
    address = AdjustCase(address.c_str());
    df.Read(city);
    city = AdjustCase(city.c_str());
    df.Read(state);
    state = StringToUpper(state.c_str());
    df.Read(phone);
    FixPhoneNumber(phone);
    df.Read(ssn);
    FixSSN(ssn);
    df.Read(description);
    df.Read(id);
    df.Read(key);

    df.Read(employee_no);
    int dept_code = 0;
    if (version <= 7)
        df.Read(dept_code);
    df.Read(training);
    df.Read(password);
    df.Read(active);

    if (version >= 7)
    {
        int count;
        df.Read(count);
        for (int i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                ReportError("Unexpected end of Job data in Employee record");
                return 1;
            }
            auto *j = new JobInfo;
            j->Read(df, version);
            if (version <= 7)
                j->dept_code = dept_code;
            Add(j);
        }
    }
    return 0;
}

int Employee::Write(OutputDataFile &df, int version)
{
    FnTrace("Employee::Write()");
    // Write version 8
    int error = 0;
    error += df.Write(system_name);
    error += df.Write(last_name);
    error += df.Write(first_name);
    error += df.Write(address);
    error += df.Write(city);
    error += df.Write(state);
    error += df.Write(phone);
    error += df.Write(ssn);
    error += df.Write(description);
    error += df.Write(id);
    error += df.Write(key);
    error += df.Write(employee_no);
    error += df.Write(training);
    error += df.Write(password);
    error += df.Write(active, 1);

    error += df.Write(JobCount());
    for (JobInfo *j = JobList(); j != nullptr; j = j->next)
        error += j->Write(df, version);
    return error;
}

int Employee::Add(JobInfo *j)
{
    FnTrace("Employee::Add()");
    return job_list.AddToTail(j);
}

int Employee::Remove(JobInfo *j)
{
    FnTrace("Employee::Remove()");
    return job_list.Remove(j);
}

JobInfo *Employee::FindJobByType(int job)
{
    FnTrace("Employee::FindJobByType()");
    JobInfo *jinfo = JobList();
    JobInfo *retval = nullptr;

    while (jinfo != nullptr)
    {
        if (jinfo->job == job)
        {
            retval = jinfo;
            jinfo = nullptr;
        }
        else
            jinfo = jinfo->next;
    }

    return retval;
}

JobInfo *Employee::FindJobByNumber(int no)
{
    FnTrace("Employee::FindJobByNumber()");
    return job_list.Index(no);
}

const char* Employee::JobTitle(Terminal *t)
{
    FnTrace("Employee::JobTitle()");
    JobInfo *j = nullptr;
    const char* retval = nullptr;

    if (last_job > 0)
    {
        j = FindJobByType(last_job);
        if (j == nullptr)
            j = JobList();
    }
    else
        j = JobList();

    if (j == nullptr)
        retval = t->Translate(UnknownStr);
    else
        retval = j->Title(t);

    return retval;
}

const char* Employee::SSN()
{
    FnTrace("Employee::SSN()");
    return ssn.Value();
}

int Employee::StartingPage()
{
    FnTrace("Employee::StartingPage()");
    JobInfo *j = FindJobByType(current_job);
    if (j == nullptr)
        return -1;
    else if (j->curr_starting_page != j->starting_page)
        return j->curr_starting_page;
    else
        return j->starting_page;
}

int Employee::SetStartingPage(int spage_id)
{
    FnTrace("Employee::SetStartingPage()");
    int retval = 1;
    JobInfo *j = FindJobByType(current_job);

    if (j != nullptr)
    {
        retval = 1;
        j->curr_starting_page = spage_id;
        retval = 0;
    }

    return retval;
}

int Employee::Security(Settings *s)
{
    FnTrace("Employee::Security()");
    if (id == 1 || id == 2)
        return 4095; // bunch of bits set to 1

    if (active == 0)
        return 0;

    int job = 0;
    if (current_job > 0)
        job = current_job;
    else if (last_job > 0)
        job = last_job;
    else if (JobList())
        job = JobList()->job;

    // allow individual security settings later
    return s->job_flags[job];
}

int Employee::IsBlank()
{
    FnTrace("Employee::IsBlank()");
    return (system_name.empty() || key <= 0);
}

int Employee::CanEdit()
{
    FnTrace("Employee::CanEdit()");
    return (id == 1 || id == 2);
}

int Employee::CanEditSystem()
{
    FnTrace("Employee::CanEditSystem()");
    return (id == 1);
}

int Employee::UseClock()
{
    FnTrace("Employee::UseClock()");
    return !(id == 1 || id == 2);
}

int Employee::UsePassword(Settings *s)
{
    FnTrace("Employee::UsePassword()");
    if (id == 1 || id == 2)
        return 0;

    int pw = s->password_mode;
    switch (pw)
    {
    case PW_NONE:
        return 0;
    case PW_MANAGERS:
        return IsManager(s);
    default:
        return 1;
    }
}

int Employee::Show(Terminal *t, int act)
{
    FnTrace("Employee::Show()");
    if (act >= 0 && active != act)
        return 0;

    for (JobInfo *j = JobList(); j != nullptr; j = j->next)
        if (j->job != JOB_NONE && ((1 << j->job) & t->job_filter) == 0)
            return 1;
    return (t->job_filter == 0);
}
