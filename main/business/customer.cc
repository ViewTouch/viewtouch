/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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
 * customer.cc - revision 6 (10/6/98)
 * Implementation of customer module
 */

#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include "customer.hh"
#include "data_file.hh"
#include "system.hh"
#include "safe_string_utils.hh"

#include "src/utils/cpp23_utils.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/*********************************************************************
 * CustomerInfo Class
 ********************************************************************/

CustomerInfo::CustomerInfo(int new_type)
{
    FnTrace("CustomerInfo::CustomerInfo()");

    next = nullptr;
    fore = nullptr;

    // Don't worry about type.  See the explanation in the
    // NewCustomerInfo() comment.
    type = new_type;
    id = -1;
    filepath.Set("");
    lastname.Set("");
    firstname.Set("");
    company.Set("");
    phone.Set("");
    extension.Set("");
    address.Set("");
    address2.Set("");
    cross_street.Set("");
    city.Set("");
    state.Set("");
    postal.Set("");
    cc_number.Set("");
    cc_expire.Set("");
    license.Set("");
    comment.Set("");
    training = 0;
    guests = 0;
}

CustomerInfo::~CustomerInfo()
= default;

int CustomerInfo::IsBlank()
{
    FnTrace("CustomerInfo::IsBlank()");
    int retval = 0;

    if (lastname.empty() &&
        firstname.empty() &&
        company.empty() &&
        phone.empty() &&
        address.empty() &&
        postal.empty() &&
        cc_number.empty())
    {
        retval = 1;
    }

    return retval;
}

int CustomerInfo::IsTraining(int set)
{
    FnTrace("CustomerInfo::IsTraining()");

    if (set >= 0)
        training = set;

    return training;
}

/****
 * SetFileName:  We have two entry points to a CustomerInfo object:  Load()
 *  and new.  In the former case, the file path should be specified in the
 *  call to Load().  In the latter case, the instantiating function should
 *  create the new CustomerInfo object and then set the path with this
 *  method.
 ****/
int CustomerInfo::SetFileName(const genericChar* filename)
{
    FnTrace("CustomerInfo::SetFileName()");
    genericChar buffer[STRLONG];

    vt::cpp23::format_to_buffer(buffer, STRLONG, "{}/customer_{}", filename, id);
    filepath.Set(buffer);
    return 0;
}

/****
 * Load:  See the note for SetFileName() for some possibly important
 *  information.
 ****/
int CustomerInfo::Load(const genericChar* filename)
{
    FnTrace("CustomerInfo::Load()");
    InputDataFile infile;
    int version = 0;
    int error = 0;

    filepath.Set(filename);
    if (infile.Open(filepath.Value(), version))
        return 1;

    error = Read(infile, version);
    infile.Close();

    return error;
}

int CustomerInfo::Save()
{
    FnTrace("CustomerInfo::Save()");
    OutputDataFile outfile;
    int error = 0;

    if (IsBlank() || training)
        return 1;

    if (outfile.Open(filepath.Value(), CUSTOMER_VERSION, 0))
        return 1;

    Write(outfile, CUSTOMER_VERSION);
    
    return error;
}

int CustomerInfo::Read(InputDataFile &df, int version)
{
    FnTrace("CustomerInfo::Read()");
    // VERSION NOTES
    // 14  (08/26/2005)   added extension, address2, cross_street
    int error = 0;

    // Kludge:  Prior to separating customers from checks the customer version
    // was the same as the check version.  When I separated customers from checks
    // I overlooked the backwards compatibility issue and decided to start customers
    // at version 1.  However, that invalidated all of the customers already existing
    // in older version checks.  To fix it, I have to set CUSTOMER_VERSION to match
    // CHECK_VERSION (as of Feb 10, 2003) and we set version 1 customers to version 13.
    if (version == 1)
        version = 13;

    if (version < 12 || (version == 12 && type != CHECK_TAKEOUT))
    {
        if (type == CHECK_RESTAURANT)
        {
            error += df.Read(table);
            error += df.Read(guests);
            error += df.Read(reserve_start);
            error += df.Read(reserve_end);
        }
        else if (type == CHECK_HOTEL)
        {
            error += df.Read(room);
            error += df.Read(guests);
            error += df.Read(lastname);
            error += df.Read(firstname);
            error += df.Read(company);
            error += df.Read(address);
            error += df.Read(city);
            error += df.Read(state);
            error += df.Read(id);
            error += df.Read(vehicle);
            error += df.Read(stay_start);
            
            if (version >= 9)
            {
                error += df.Read(stay_end);
                error += df.Read(phone);
                error += df.Read(comment);
            }
            else
            {
                int len = 0;
                error += df.Read(len);
                
                stay_end.Set(stay_start);
                if (len > 0)
                    stay_end += date::days(len);
            }
        }
    }
    else if ((version == 12 && type == CHECK_TAKEOUT) || (version >= 13))
    {
        error += df.Read(id);
        error += df.Read(lastname);
        error += df.Read(firstname);
        error += df.Read(company);
        error += df.Read(phone);
        error += df.Read(address);
        error += df.Read(city);
        error += df.Read(state);
        error += df.Read(postal);
        error += df.Read(cc_number);
        error += df.Read(cc_expire);
        error += df.Read(license);
        error += df.Read(comment);
    }
    else if (debug_mode)
    {
        printf("Weird customer version:  %d\n", version);
    }

    if (version >= 14)
    {
        error += df.Read(extension);
        error += df.Read(address2);
        error += df.Read(cross_street);
    }

    return error;
}

int CustomerInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("CustomerInfo::Write()");
    int error = 0;

    error += df.Write(id);
    error += df.Write(lastname);
    error += df.Write(firstname);
    error += df.Write(company);
    error += df.Write(phone);
    error += df.Write(address);
    error += df.Write(city);
    error += df.Write(state);
    error += df.Write(postal);
    error += df.Write(cc_number);
    error += df.Write(cc_expire);
    error += df.Write(license);
    error += df.Write(comment);
    error += df.Write(extension);
    error += df.Write(address2);
    error += df.Write(cross_street);

    return error;
}

int CustomerInfo::DeleteFile()
{
    FnTrace("CustomerInfo::DeleteFile()");
    int retval = 1;

    if (filepath.size() > 0)
    {
        unlink(filepath.Value());
        retval = 0;
    }
    return retval;
}

int CustomerInfo::Search(const char* word)
{
    FnTrace("CustomerInfo::Search()");
    int retval = 0;

    if (strlen(word) < 1)
        retval = 0;
    else if (StringInString(lastname.Value(), word))
        retval = 1;
    else if (StringInString(firstname.Value(), word))
        retval = 2;
    else if (StringInString(company.Value(), word))
        retval = 3;
    else if (StringInString(phone.Value(), word))
        retval = 4;
    else if (StringInString(address.Value(), word))
        retval = 5;
    else if (StringInString(comment.Value(), word))
        retval = 6;

    return retval;
}

int CustomerInfo::Guests(int set)
{
    FnTrace("CustomerInfo::Guests()");

    if (set > -1)
        guests = set;

    return guests;
}

const genericChar* CustomerInfo::LastName(const char* set)
{
    FnTrace("CustomerInfo::LastName()");

    if (set != nullptr)
        lastname.Set(set);

    return lastname.Value();
}

const genericChar* CustomerInfo::FirstName(const char* set)
{
    FnTrace("CustomerInfo::FirstName()");

    if (set != nullptr)
        firstname.Set(set);

    return firstname.Value();
}

const genericChar* CustomerInfo::Company(const char* set)
{
    FnTrace("CustomerInfo::Company()");

    if (set != nullptr)
        company.Set(set);

    return company.Value();
}

const genericChar* CustomerInfo::PhoneNumber(const char* set)
{
    FnTrace("CustomerInfo::PhoneNumber()");

    if (set != nullptr)
        phone.Set(set);

    return phone.Value();
}

const genericChar* CustomerInfo::Extension(const char* set)
{
    FnTrace("CustomerInfo::Extension()");

    if (set != nullptr)
        extension.Set(set);

    return extension.Value();
}

const genericChar* CustomerInfo::Address(const char* set)
{
    FnTrace("CustomerInfo::Address()");

    if (set != nullptr)
        address.Set(set);

    return address.Value();
}

const genericChar* CustomerInfo::Address2(const char* set)
{
    FnTrace("CustomerInfo::Address2()");

    if (set != nullptr)
        address2.Set(set);

    return address2.Value();
}

const genericChar* CustomerInfo::CrossStreet(const char* set)
{
    FnTrace("CustomerInfo::CrossStreet()");

    if (set != nullptr)
        cross_street.Set(set);

    return cross_street.Value();
}

const genericChar* CustomerInfo::City(const char* set)
{
    FnTrace("CustomerInfo::City()");

    if (set != nullptr)
        city.Set(set);

    return city.Value();
}

const genericChar* CustomerInfo::State(const char* set)
{
    FnTrace("CustomerInfo::State()");

    if (set != nullptr)
        state.Set(set);

    return state.Value();
}

const genericChar* CustomerInfo::Postal(const char* set)
{
    FnTrace("CustomerInfo::Postal()");

    if (set != nullptr)
        postal.Set(set);

    return postal.Value();
}

const genericChar* CustomerInfo::License(const char* set)
{
    FnTrace("CustomerInfo::License()");

    if (set != nullptr)
        license.Set(set);

    return license.Value();
}

const genericChar* CustomerInfo::CCNumber(const char* set)
{
    FnTrace("CustomerInfo::CCNumber()");

    if (set != nullptr)
        cc_number.Set(set);

    return cc_number.Value();
}

const genericChar* CustomerInfo::CCExpire(const char* set)
{
    FnTrace("CustomerInfo::CCExpire()");

    if (set != nullptr)
        cc_expire.Set(set);

    return cc_expire.Value();
}

const genericChar* CustomerInfo::Comment(const char* set)
{
    FnTrace("CustomerInfo::Comment()");

    if (set != nullptr)
        comment.Set(set);

    return comment.Value();
}

const genericChar* CustomerInfo::Vehicle(const char* set)
{
    FnTrace("CustomerInfo::Vehicle()");

    if (set != nullptr)
        vehicle.Set(set);

    return vehicle.Value();
}


/*********************************************************************
 * CustomerInfoDB Class
 ********************************************************************/

CustomerInfoDB::CustomerInfoDB()
{
    FnTrace("CustomerInfoDB::CustomerInfoDB()");
    last_id = -1;
}

CustomerInfoDB::~CustomerInfoDB()
= default;

int CustomerInfoDB::RemoveBlank()
{
    FnTrace("CustomerInfoDB::RemoveBlank()");
    int retval = 1;
    CustomerInfo *customer = customers.Head();

    while (customer != nullptr)
    {
        if (customer->IsBlank())
        {
            customers.RemoveSafe(customer);
            customer = customers.Head();
        }
        else
        {
            customer = customer->next;
        }
    }

    return retval;
}

/****
 * Count:  Counts all of the non-blank records and returns the result.
 ****/
int CustomerInfoDB::Count()
{
    FnTrace("CustomerInfoDB::Count()");
    int count = 0;
    CustomerInfo *customer = customers.Head();

    while (customer != nullptr)
    {
        count += 1;
        customer = customer->next;
    }

    return count;
}

int CustomerInfoDB::Save(const genericChar* filepath)
{
    FnTrace("CustomerInfoDB::Save(genericChar)");
    int retval = 1;
    CustomerInfo *customer = customers.Head();

    if (filepath != nullptr)
        pathname.Set(filepath);

    while (customer != nullptr)
    {
        if (customer->id < 0)
            customer->id = NextID();
        customer->Save();
        customer = customer->next;
    }

    return retval;
}

int CustomerInfoDB::Save(CustomerInfo *customer)
{
    FnTrace("CustomerInfoDB::Save(CustomerInfo)");
    int retval = 1;

    if (customer->id < 0)
        customer->id = NextID();
    customer->Save();

    return retval;
}

int CustomerInfoDB::Load(const genericChar* filepath)
{
    FnTrace("CustomerInfoDB::Load()");
    int retval = 0;
    DIR *dp;
    struct dirent *record = nullptr;
    genericChar buffer[STRLONG];

    if (filepath)
        pathname.Set(filepath);

    if (pathname.empty())
        return 1;

    dp = opendir(pathname.Value());
    if (dp == nullptr)
        return 1;  // Error - can't find directory

    do
    {
        record = readdir(dp);
        if (record)
        {
            const genericChar* name = record->d_name;
            if (strncmp("customer_", name, 9) == 0)
            {
                vt_safe_string::safe_format(buffer, STRLONG, "%s/%s", pathname.Value(), name);
                auto *custinfo = new CustomerInfo();
                if (custinfo->Load(buffer))
                    ReportError("Error loading customer");
                else
                {
                    Add(custinfo);
                    if (custinfo->id > last_id)
                        last_id = custinfo->id;
                }
            }
        }
    }
    while (record);

    closedir(dp);
    return retval;
}

CustomerInfo *CustomerInfoDB::NewCustomer(int type)
{
    FnTrace("CustomerInfoDB::NewCustomer()");

    auto *ci = new CustomerInfo(type);
    if (ci != nullptr)
    {
        Add(ci);
        ci->SetFileName(pathname.Value());
    }

    return ci;
}

int CustomerInfoDB::Add(CustomerInfo *customer)
{
    FnTrace("CustomerInfoDB::Add()");
    int retval = 0;

    if (customer->id < 0)
        customer->id = NextID();
    customers.AddToTail(customer);

    return retval;
}

int CustomerInfoDB::Remove(CustomerInfo *customer)
{
    FnTrace("CustomerInfoDB::Remove()");
    int retval = 1;

    customer->DeleteFile();
    customers.Remove(customer);

    return retval;
}

CustomerInfo *CustomerInfoDB::FindByID(int customer_id)
{
    FnTrace("CustomerInfoDB::FindByID()");
    CustomerInfo *retval = nullptr;
    CustomerInfo *customer = customers.Head();

    if (customer_id < 0)
        return retval;

    while (customer != nullptr)
    {
        if (customer_id == customer->id)
        {
            retval = customer;
            customer = nullptr;
        }
        else
            customer = customer->next;
    }

    return retval;
}

CustomerInfo *CustomerInfoDB::FindByString(const genericChar* search_string, int start)
{
    FnTrace("CustomerInfoDB::FindByString()");
    CustomerInfo *retval = nullptr;
    CustomerInfo *customer = customers.Head();
    CustomerInfo *first_customer = nullptr;
    int done = 0;

    if (start > -1)
    {
        while (customer != nullptr && customer->id <= start)
            customer = customer->next;
        if (customer == nullptr)
            customer = customers.Head();
    }

    first_customer = customer;
    while (customer != nullptr && done != 1)
    {
        if (customer->Search(search_string))
        {
            retval = customer;
            done = 1;
        }
        else
        {
            customer = customer->next;
            if (customer == nullptr)
                customer = customers.Head();
            if (customer == first_customer)
                done = 1;
        }
    }

    return retval;
}

CustomerInfo *CustomerInfoDB::FindBlank()
{
    FnTrace("CustomerInfoDB::FindBlank()");
    CustomerInfo *retval = nullptr;
    CustomerInfo *customer = customers.Tail();
    
    while (customer != nullptr)
    {
        if (customer->IsBlank())
        {
            retval = customer;
            customer = nullptr;
        }
        else
        {
            customer = customer->fore;
        }
    }

    return retval;
}

/*********************************************************************
 * General Functions
 ********************************************************************/

/****
 * NewCustomerInfo:  type is not currently used (29Jan2003).  It used
 *  to hold the check type, but that didn't work well here for multiple
 *  checks per customer, so I've moved type to the Check class.  I've
 *  left the argument here partly because I may bring it back in a
 *  different form.  BAK
 ****/
CustomerInfo *NewCustomerInfo(int type)
{
    FnTrace("NewCustomerInfo()");

    return MasterSystem->customer_db.NewCustomer(type);
}
