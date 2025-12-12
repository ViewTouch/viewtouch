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
 * locale.cc - revision 35 (10/7/98)
 * Implementation of locale module
 */

#include "locale.hh"
#include "data_file.hh"
#include "labels.hh"
#include "employee.hh"
#include "settings.hh"
#include "system.hh"
#include "terminal.hh"
#include "manager.hh"
#include "safe_string_utils.hh"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <clocale>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

// Global current language variable for static translation function
static int global_current_language = LANG_ENGLISH;

// Translation arrays for common UI strings
struct TranslationEntry {
    const char* english;
    const char* spanish;
};

static const TranslationEntry common_translations[] = {
    // Common buttons and actions
    {"Okay", "Aceptar"},
    {"Cancel", "Cancelar"},
    {"Yes", "Sí"},
    {"No", "No"},
    {"Continue", "Continuar"},
    {"Start", "Iniciar"},
    {"Stop", "Detener"},
    {"Save", "Guardar"},
    {"Delete", "Eliminar"},
    {"Edit", "Editar"},
    {"Add", "Agregar"},
    {"Remove", "Remover"},
    {"Enter", "Entrar"},
    {"Backspace", "Retroceso"},
    {"Clear", "Limpiar"},
    {"Change", "Cambiar"},
    {"Print", "Imprimir"},
    {"Next", "Siguiente"},
    {"Prior", "Anterior"},
    {"Done", "Hecho"},
    {"Quit", "Salir"},
    {"Exit", "Salir"},
    {"Logout", "Cerrar Sesión"},
    {"Close", "Cerrar"},
    {"Open", "Abrir"},
    {"New", "Nuevo"},
    {"Search", "Buscar"},
    {"Find", "Encontrar"},
    {"Replace", "Reemplazar"},
    {"Copy", "Copiar"},
    {"Paste", "Pegar"},
    {"Cut", "Cortar"},
    {"Undo", "Deshacer"},
    {"Redo", "Rehacer"},
    {"Refresh", "Actualizar"},
    {"Reload", "Recargar"},
    {"Reset", "Reiniciar"},
    {"Apply", "Aplicar"},
    {"Submit", "Enviar"},
    {"Send", "Enviar"},
    {"Receive", "Recibir"},
    {"Accept", "Aceptar"},
    {"Reject", "Rechazar"},
    {"Approve", "Aprobar"},
    {"Deny", "Denegar"},
    {"Confirm", "Confirmar"},
    {"Verify", "Verificar"},
    {"Validate", "Validar"},
    {"Check", "Verificar"},
    {"Test", "Probar"},
    {"Run", "Ejecutar"},
    {"Execute", "Ejecutar"},
    {"Process", "Procesar"},
    {"Complete", "Completar"},
    {"Finish", "Finalizar"},
    {"End", "Terminar"},
    {"Begin", "Comenzar"},
    {"Start", "Iniciar"},

    // Navigation and menus
    {"Menu", "Menú"},
    {"Settings", "Configuración"},
    {"Reports", "Reportes"},
    {"Orders", "Pedidos"},
    {"Payments", "Pagos"},
    {"Tables", "Mesas"},
    {"Users", "Usuarios"},
    {"System", "Sistema"},
    {"Help", "Ayuda"},
    {"Welcome", "Bienvenido"},
    {"Hello", "Hola"},
    {"Please", "Por favor"},
    {"Select", "Seleccionar"},
    {"Choose", "Elegir"},
    {"Click", "Hacer clic"},
    {"Press", "Presionar"},
    {"Touch", "Tocar"},
    {"Home", "Inicio"},
    {"Back", "Atrás"},
    {"Forward", "Adelante"},
    {"Up", "Arriba"},
    {"Down", "Abajo"},
    {"Left", "Izquierda"},
    {"Right", "Derecha"},
    {"Top", "Superior"},
    {"Bottom", "Inferior"},
    {"First", "Primero"},
    {"Last", "Último"},
    {"Previous", "Anterior"},
    {"Page", "Página"},
    {"Screen", "Pantalla"},
    {"Window", "Ventana"},
    {"Dialog", "Diálogo"},
    {"Message", "Mensaje"},
    {"Alert", "Alerta"},
    {"Warning", "Advertencia"},
    {"Error", "Error"},
    {"Info", "Información"},
    {"Status", "Estado"},
    {"Progress", "Progreso"},
    {"Loading", "Cargando"},
    {"Waiting", "Esperando"},
    {"Processing", "Procesando"},
    {"Connecting", "Conectando"},
    {"Connected", "Conectado"},
    {"Disconnected", "Desconectado"},
    {"Online", "En línea"},
    {"Offline", "Fuera de línea"},
    {"Available", "Disponible"},
    {"Unavailable", "No disponible"},
    {"Enabled", "Habilitado"},
    {"Disabled", "Deshabilitado"},
    {"Active", "Activo"},
    {"Inactive", "Inactivo"},
    {"Visible", "Visible"},
    {"Hidden", "Oculto"},
    {"Show", "Mostrar"},
    {"Hide", "Ocultar"},
    {"Expand", "Expandir"},
    {"Collapse", "Colapsar"},
    {"Minimize", "Minimizar"},
    {"Maximize", "Maximizar"},
    {"Restore", "Restaurar"},
    {"Zoom", "Zoom"},
    {"Full Screen", "Pantalla Completa"},

    // Time and date
    {"Time", "Hora"},
    {"Date", "Fecha"},
    {"Today", "Hoy"},
    {"Yesterday", "Ayer"},
    {"Tomorrow", "Mañana"},
    {"Now", "Ahora"},
    {"Later", "Más tarde"},
    {"Soon", "Pronto"},
    {"Today", "Hoy"},
    {"This Week", "Esta Semana"},
    {"This Month", "Este Mes"},
    {"This Year", "Este Año"},
    {"Last Week", "Semana Pasada"},
    {"Last Month", "Mes Pasado"},
    {"Last Year", "Año Pasado"},
    {"Next Week", "Próxima Semana"},
    {"Next Month", "Próximo Mes"},
    {"Next Year", "Próximo Año"},
    {"AM", "AM"},
    {"PM", "PM"},
    {"Morning", "Mañana"},
    {"Afternoon", "Tarde"},
    {"Evening", "Noche"},
    {"Night", "Noche"},
    {"Hour", "Hora"},
    {"Minute", "Minuto"},
    {"Second", "Segundo"},
    {"Day", "Día"},
    {"Week", "Semana"},
    {"Month", "Mes"},
    {"Year", "Año"},
    {"Monday", "Lunes"},
    {"Tuesday", "Martes"},
    {"Wednesday", "Miércoles"},
    {"Thursday", "Jueves"},
    {"Friday", "Viernes"},
    {"Saturday", "Sábado"},
    {"Sunday", "Domingo"},

    // Financial and payment terms
    {"Cash", "Efectivo"},
    {"Credit", "Crédito"},
    {"Debit", "Débito"},
    {"Check", "Cheque"},
    {"Money", "Dinero"},
    {"Payment", "Pago"},
    {"Amount", "Monto"},
    {"Total", "Total"},
    {"Subtotal", "Subtotal"},
    {"Tax", "Impuestos"},
    {"Price", "Precio"},
    {"Cost", "Costo"},
    {"Fee", "Tarifa"},
    {"Charge", "Cargo"},
    {"Discount", "Descuento"},
    {"Tip", "Propina"},
    {"Change", "Cambio"},
    {"Balance", "Saldo"},
    {"Due", "Adeudado"},
    {"Owed", "Adeudado"},
    {"Paid", "Pagado"},
    {"Refund", "Reembolso"},
    {"Receipt", "Recibo"},
    {"Invoice", "Factura"},
    {"Bill", "Factura"},
    {"Account", "Cuenta"},
    {"Customer", "Cliente"},
    {"Vendor", "Proveedor"},
    {"Supplier", "Proveedor"},
    {"Purchase", "Compra"},
    {"Sale", "Venta"},
    {"Transaction", "Transacción"},
    {"Order", "Pedido"},
    {"Item", "Artículo"},
    {"Product", "Producto"},
    {"Service", "Servicio"},
    {"Quantity", "Cantidad"},
    {"Unit", "Unidad"},
    {"Package", "Paquete"},
    {"Bundle", "Paquete"},
    {"Delivery", "Entrega"},
    {"Shipping", "Envío"},
    {"Address", "Dirección"},
    {"Location", "Ubicación"},

    // System and technical terms
    {"System", "Sistema"},
    {"Computer", "Computadora"},
    {"Server", "Servidor"},
    {"Client", "Cliente"},
    {"Network", "Red"},
    {"Internet", "Internet"},
    {"Connection", "Conexión"},
    {"Database", "Base de datos"},
    {"File", "Archivo"},
    {"Folder", "Carpeta"},
    {"Directory", "Directorio"},
    {"Drive", "Unidad"},
    {"Memory", "Memoria"},
    {"Storage", "Almacenamiento"},
    {"Disk", "Disco"},
    {"USB", "USB"},
    {"Printer", "Impresora"},
    {"Scanner", "Escáner"},
    {"Keyboard", "Teclado"},
    {"Mouse", "Ratón"},
    {"Screen", "Pantalla"},
    {"Monitor", "Monitor"},
    {"Display", "Display"},
    {"Terminal", "Terminal"},
    {"Device", "Dispositivo"},
    {"Hardware", "Hardware"},
    {"Software", "Software"},
    {"Program", "Programa"},
    {"Application", "Aplicación"},
    {"Version", "Versión"},
    {"Update", "Actualización"},
    {"Install", "Instalar"},
    {"Setup", "Configuración"},
    {"Configuration", "Configuración"},
    {"Settings", "Configuración"},
    {"Options", "Opciones"},
    {"Preferences", "Preferencias"},
    {"Default", "Predeterminado"},
    {"Custom", "Personalizado"},
    {"Advanced", "Avanzado"},
    {"Basic", "Básico"},
    {"Automatic", "Automático"},
    {"Manual", "Manual"},
    {"On", "Encendido"},
    {"Off", "Apagado"},
    {"True", "Verdadero"},
    {"False", "Falso"},
    {"Yes", "Sí"},
    {"No", "No"},
    {"Enable", "Habilitar"},
    {"Disable", "Deshabilitar"},
    {"Lock", "Bloquear"},
    {"Unlock", "Desbloquear"},
    {"Secure", "Seguro"},
    {"Password", "Contraseña"},
    {"Login", "Iniciar Sesión"},
    {"Username", "Nombre de Usuario"},
    {"User", "Usuario"},
    {"Admin", "Administrador"},
    {"Manager", "Gerente"},
    {"Employee", "Empleado"},
    {"Customer", "Cliente"},
    {"Guest", "Invitado"},

    // Restaurant/hospitality specific
    {"Table", "Mesa"},
    {"Seat", "Asiento"},
    {"Guest", "Invitado"},
    {"Party", "Grupo"},
    {"Reservation", "Reservación"},
    {"Wait", "Espera"},
    {"Service", "Servicio"},
    {"Waiter", "Mesero"},
    {"Waitress", "Mesera"},
    {"Server", "Mesero"},
    {"Bartender", "Bartender"},
    {"Cook", "Cocinero"},
    {"Chef", "Chef"},
    {"Manager", "Gerente"},
    {"Host", "Anfitrión"},
    {"Hostess", "Anfitriona"},
    {"Kitchen", "Cocina"},
    {"Bar", "Bar"},
    {"Restaurant", "Restaurante"},
    {"Dining", "Comedor"},
    {"Takeout", "Para Llevar"},
    {"Delivery", "Entrega"},
    {"Catering", "Catering"},
    {"Buffet", "Buffet"},
    {"Breakfast", "Desayuno"},
    {"Lunch", "Almuerzo"},
    {"Dinner", "Cena"},
    {"Brunch", "Brunch"},
    {"Appetizer", "Entrada"},
    {"Entree", "Plato Principal"},
    {"Dessert", "Postre"},
    {"Beverage", "Bebida"},
    {"Drink", "Bebida"},
    {"Wine", "Vino"},
    {"Beer", "Cerveza"},
    {"Cocktail", "Cóctel"},
    {"Coffee", "Café"},
    {"Tea", "Té"},
    {"Juice", "Jugo"},
    {"Water", "Agua"},
    {"Soda", "Refresco"},
    {"Food", "Comida"},
    {"Meal", "Comida"},
    {"Dish", "Plato"},
    {"Plate", "Plato"},
    {"Course", "Plato"},
    {"Special", "Especial"},
    {"Daily", "Diario"},
    {"Weekly", "Semanal"},
    {"Monthly", "Mensual"},

    // Status and feedback messages
    {"Success", "Éxito"},
    {"Failure", "Fallo"},
    {"Error", "Error"},
    {"Warning", "Advertencia"},
    {"Information", "Información"},
    {"Notice", "Aviso"},
    {"Loading", "Cargando"},
    {"Saving", "Guardando"},
    {"Deleting", "Eliminando"},
    {"Processing", "Procesando"},
    {"Please wait", "Por favor espere"},
    {"Working", "Trabajando"},
    {"Ready", "Listo"},
    {"Busy", "Ocupado"},
    {"Complete", "Completo"},
    {"Incomplete", "Incompleto"},
    {"Valid", "Válido"},
    {"Invalid", "Inválido"},
    {"Correct", "Correcto"},
    {"Incorrect", "Incorrecto"},
    {"Required", "Requerido"},
    {"Optional", "Opcional"},
    {"Empty", "Vacío"},
    {"Full", "Lleno"},
    {"Available", "Disponible"},
    {"Unavailable", "No disponible"},
    {"Out of stock", "Agotado"},
    {"In stock", "En stock"},
    {"Low stock", "Stock bajo"},
    {"High stock", "Stock alto"},

    // Error and system messages
    {"Cannot process unknown code: %d", "No se puede procesar código desconocido: %d"},
    {"Last code processed was %d", "Último código procesado fue %d"},
    {"Unable to find jump target (%d, %d) for %s", "No se puede encontrar objetivo de salto (%d, %d) para %s"},
    {"Unknown index - can't jump", "Índice desconocido - no se puede saltar"},
    {"ALERT: Page stack size exceeded", "ALERTA: Tamaño de pila de página excedido"},
    {"Select a Bar Tab", "Seleccionar una Pestaña de Bar"},
    {"Connection reset.\\Please wait 60 seconds\\and try again.", "Conexión restablecida.\\Por favor espere 60 segundos\\e intente nuevamente."},
    {"Scheduled restart postponed for 1 hour", "Reinicio programado pospuesto por 1 hora"},
    {"Button images %s on this terminal", "Imágenes de botones %s en esta terminal"},
    {"ENABLED", "HABILITADO"},
    {"DISABLED", "DESHABILITADO"},
    {"Someone else is already in Edit Mode", "Alguien más ya está en Modo de Edición"},
    {"System Page - Can't Edit", "Página del Sistema - No se puede Editar"},
    {"Couldn't jump to page %d", "No se pudo saltar a la página %d"},
    {"Cannot export pages while in edit mode.", "No se pueden exportar páginas mientras está en modo de edición."},
    {"Also clear labor data?", "¿También limpiar datos laborales?"},
    {"F3/F4", "F3/F4"},
    {"Language changed to: %s", "Idioma cambiado a: %s"},
    {"Customer Discounts", "Descuentos de Cliente"},
    {"Coupons", "Cupones"},

    // Check.cc error messages
    {"Unexpected end of orders in SubCheck", "Fin inesperado de pedidos en SubCheque"},
    {"Error in adding order", "Error al agregar pedido"},
    {"Unexpected end of payments in SubCheck", "Fin inesperado de pagos en SubCheque"},
    {"Error in adding payment", "Error al agregar pago"},

    // Manager.cc system messages
    {"Can't open initial loader socket", "No se puede abrir socket inicial del cargador"},
    {"Can't connect to loader", "No se puede conectar al cargador"},
    {"Couldn't create main system object", "No se pudo crear objeto principal del sistema"},
    {"Automatic check for updates...", "Verificación automática de actualizaciones..."},
    {"Auto-update of vt_data is disabled in settings", "Actualización automática de vt_data está deshabilitada en configuración"},
    {"Auto-update of vt_data is enabled in settings", "Actualización automática de vt_data está habilitada en configuración"},
    {"Warning: Could not load settings file, defaulting to auto-update enabled", "Advertencia: No se pudo cargar archivo de configuración, por defecto actualización automática habilitada"},
    {"Warning: Settings file not found, defaulting to auto-update enabled", "Advertencia: Archivo de configuración no encontrado, por defecto actualización automática habilitada"},
    {"Local vt_data not found, attempting to download from update servers...", "vt_data local no encontrado, intentando descargar desde servidores de actualización..."},
    {"Unknown signal %d received", "Señal desconocida %d recibida"},
    {"Can't find path '%s'", "No se puede encontrar ruta '%s'"},
    {"Scheduled Restart Time\\System needs to restart now.\\Choose an option:", "Tiempo de Reinicio Programado\\El sistema necesita reiniciarse ahora.\\Elija una opción:"},
    {"Restart Now", "Reiniciar Ahora"},
    {"Postpone 1 Hour", "Posponer 1 Hora"},

    // Credit.cc transaction types
    {"==== TRANSACTION RECORD ====", "==== REGISTRO DE TRANSACCIÓN ===="},
    {"Purchase", "Compra"},
    {"Pre-Authorization", "Pre-Autorización"},
    {"Pre-Auth Completion", "Completación Pre-Aut"},
    {"Pre-Auth Advice", "Aviso Pre-Aut"},
    {"Refund", "Reembolso"},
    {"Refund Cancel", "Cancelar Reembolso"},
    {"Purchase Correction", "Corrección de Compra"},
    {"Void Cancel", "Cancelar Anulación"},

    // Labor zone terms
    {"Start", "Inicio"},
    {"End", "Fin"},
    {"Clock Out", "Salir del Reloj"},
    {"Start Break", "Iniciar Descanso"},
    {"Job", "Trabajo"},
    {"Pay", "Pagar"},
    {"Rate", "Tarifa"},
    {"Tips", "Propinas"},
    {"Time Clock Summary", "Resumen de Reloj de Tiempo"},

    // User edit zone terms
    {"User ID", "ID de Usuario"},
    {"Nickname", "Apodo"},
    {"Last Name", "Apellido"},
    {"First Name", "Nombre"},
    {"Address", "Dirección"},
    {"City", "Ciudad"},
    {"State", "Estado"},
    {"Job Info", "Información del Trabajo"},
    {"Employee #", "Empleado #"},
    {"Pay Rate", "Tarifa de Pago"},
    {"Start Page", "Página de Inicio"},
    {"Department", "Departamento"},
    {"Remove This Job", "Remover Este Trabajo"},
    {"* Add Another Job *", "* Agregar Otro Trabajo *"},
    {"Filtered Active Employees", "Empleados Activos Filtrados"},
    {"Filtered Inactive Employees", "Empleados Inactivos Filtrados"},
    {"All Active Employees", "Todos los Empleados Activos"},
    {"All Inactive Employees", "Todos los Empleados Inactivos"},
    {"Employee Record", "Registro del Empleado"},

    // Order zone terms
    {"To Go", "Para Llevar"},
    {"Here", "Aquí"},
    {"TO ", "PARA "},
    {"COMP", "COMP"},
    {"Next\\Seat", "Siguiente\\Asiento"},
    {"Prior\\Seat", "Anterior\\Asiento"},
    {"Next\\Check", "Siguiente\\Cheque"},
    {"Prior\\Check", "Anterior\\Cheque"},
    {"Order Entry", "Entrada de Pedido"},

    {nullptr, nullptr} // End marker
};

// Function to lookup translation in the hardcoded array
static const char* LookupHardcodedTranslation(const char* str, int lang) {
    if (lang != LANG_SPANISH) {
        return nullptr; // Only Spanish translations in the array
    }

    for (const TranslationEntry* entry = common_translations; entry->english != nullptr; ++entry) {
        if (strcmp(str, entry->english) == 0) {
            return entry->spanish;
        }
    }

    return nullptr; // Not found in hardcoded translations
}

void StartupLocalization()
{
    if (setlocale(LC_ALL, "") == NULL)
    {
	    (void) fprintf(stderr, "Cannot set locale.\n");
	    exit(1);
	}
}

// Global translation function that can be used anywhere
const genericChar* GlobalTranslate(const genericChar* str)
{
    if (MasterLocale == NULL)
        return str;

    return MasterLocale->Translate(str, global_current_language, 0);
}


/**** Global Data ****/
std::unique_ptr<Locale> MasterLocale = nullptr;

void SetGlobalLanguage(int language)
{
    global_current_language = language;
}

int GetGlobalLanguage()
{
    return global_current_language;
}


/**** Phrase Data (default U.S. English) ****/
#define PHRASE_SUNDAY    0
#define PHRASE_MONDAY    1
#define PHRASE_TUESDAY   2
#define PHRASE_WEDNESDAY 3
#define PHRASE_THURSDAY  4
#define PHRASE_FRIDAY    5
#define PHRASE_SATURDAY  6

#define PHRASE_SUN       7
#define PHRASE_MON       8
#define PHRASE_TUE       9
#define PHRASE_WED       10
#define PHRASE_THU       11
#define PHRASE_FRI       12
#define PHRASE_SAT       13

#define PHRASE_JANUARY   14
#define PHRASE_FEBRUARY  15
#define PHRASE_MARCH     16
#define PHRASE_APRIL     17
#define PHRASE_MAY       18
#define PHRASE_JUNE      19
#define PHRASE_JULY      20
#define PHRASE_AUGUST    21
#define PHRASE_SEPTEMBER 22
#define PHRASE_OCTOBER   23
#define PHRASE_NOVEMBER  24
#define PHRASE_DECEMBER  25

#define PHRASE_M1        26
#define PHRASE_M2        27
#define PHRASE_M3        28
#define PHRASE_M4        29
#define PHRASE_M5        30
#define PHRASE_M6        31
#define PHRASE_M7        32
#define PHRASE_M8        33
#define PHRASE_M9        34
#define PHRASE_M10       35
#define PHRASE_M11       36
#define PHRASE_M12       37

#define PHRASE_YES               38
#define PHRASE_NO                39
#define PHRASE_ON                40
#define PHRASE_OFF               41

static const genericChar* AMorPM[] = { "am", "pm"};

PhraseEntry PhraseData[] = {
    // Days of Week (0 - 6)
    {0, "Sunday"},
    {0, "Monday"},
    {0, "Tuesday"},
    {0, "Wednesday"},
    {0, "Thursday"},
    {0, "Friday"},
    {0, "Saturday"},
    // Abrv. Days of Week (7 - 13)
    {1, "Sun"},
    {1, "Mon"},
    {1, "Tue"},
    {1, "Wed"},
    {1, "Thu"},
    {1, "Fri"},
    {1, "Sat"},
    // Months (14 - 25)
    {2, "January"},
    {2, "February"},
    {2, "March"},
    {2, "April"},
    {2, "May"},
    {2, "June"},
    {2, "July"},
    {2, "August"},
    {2, "September"},
    {2, "October"},
    {2, "November"},
    {2, "December"},
    // Abrv. Months (26 - 37)
    {3, "Jan"},
    {3, "Feb"},
    {3, "Mar"},
    {3, "Apr"},
    {3, "May"},
    {3, "Jun"},
    {3, "Jul"},
    {3, "Aug"},
    {3, "Sep"},
    {3, "Oct"},
    {3, "Nov"},
    {3, "Dec"},
    // General (38 - 41)
    {4, "Yes"},
    {4, "No"},
    {4, "On"},
    {4, "Off"},
    {4, "Page"},
    {4, "Table"},
    {4, "Guests"},
    {4, "Okay"},
    {4, "Cancel"},
    {4, "Take Out"},
    {4, "TO GO"},
    {4, "Catering"},
    {4, "Cater"},
    {4, "Delivery"},
    {4, "Deliver"},
    {4, "PENDING"},
    // Greetings (42 - 43)
    {5, "Welcome"},
    {5, "Hello"},
    // Statements (44 - 45)
    {6, "Starting Time Is"},
    {6, "Ending Time Is"},
    {6, "Pick A Job For This Shift"},
    // Commands (46 - 48)
    {7, "Please Enter Your User ID"},
    {7, "Press START To Enter"},
    {7, "Please Try Again"},
    {7, "Contact a manager to be reactivated"},
    // Errors (49 - 56)
    {8, "Password Incorrect"},
    {8, "Unknown User ID"},
    {8, "You're Using Another Terminal"},
    {8, "You're Not On The Clock"},
    {8, "You're Already On The Clock"},
    {8, "You Don't Use The Clock"},
    {8, "You Still Have Open Checks"},
    {8, "You Still Have An Assigned Drawer"},
    {8, "Your Record Is Inactive"},
    // Index Pages
    {9, "General"},
    {9, "Breakfast"},
    {9, "Brunch"},
    {9, "Lunch"},
    {9, "Early Dinner"},
    {9, "Dinner"},
    {9, "Late Night"},
    {9, "Bar"},
    {9, "Wine"},
    {9, "Cafe"},
    // Jobs
    {10, "No Job"},
    {10, "Dishwasher"},
    {10, "Busperson"},
    {10, "Line Cook"},
    {10, "Prep Cook"},
    {10, "Chef"},
    {10, "Cashier"},
    {10, "Server"},
    {10, "Server/Cashier"},
    {10, "Bartender"},
    {10, "Host/Hostess"},
    {10, "Bookkeeper"},
    {10, "Supervisor"},
    {10, "Assistant Manager"},
    {10, "Manager"},
    // Families
    {11, FamilyName[0]},
    {11, FamilyName[1]},
    {11, FamilyName[2]},
    {11, FamilyName[3]},
    {11, FamilyName[4]},
    {11, FamilyName[5]},
    {11, FamilyName[6]},
    {11, FamilyName[7]},
    {11, FamilyName[8]},
    {11, FamilyName[9]},
    {11, FamilyName[10]},
    {11, FamilyName[11]},
    {11, FamilyName[12]},
    {11, FamilyName[13]},
    {11, FamilyName[14]},
    {11, FamilyName[15]},
    {12, FamilyName[16]},
    {12, FamilyName[17]},
    {12, FamilyName[18]},
    {12, FamilyName[19]},
    {12, FamilyName[20]},
    {12, FamilyName[21]},
    {12, FamilyName[22]},
    {12, FamilyName[23]},
    {12, FamilyName[24]},
    {12, FamilyName[25]},
    {12, FamilyName[26]},
    {12, FamilyName[27]},
    {12, FamilyName[28]},
    {12, FamilyName[29]},
    {12, FamilyName[30]},

    {13, "Pre-Authorize"},
    {13, "Authorize"},
    {13, "Void"},
    {13, "Refund"},
    {13, "Add Tip"},
    {13, "Cancel"},
    {13, "Undo Refund"},
    {13, "Manual Entry"},
    {13, "Done"},
    {13, "Credit"},
    {13, "Debit"},
    {13, "Swipe"},
    {13, "Clear"},
    {13, "Card Number"},
    {13, "Expires"},
    {13, "Holder"},
    {14, "Charge Amount"},
    {14, "Tip Amount"},
    {14, "Total"},
    {14, "Void Successful"},
    {14, "Refund Successful"},
    {14, "Please select card type."},
    {14, "Please select card entry method."},
    {14, "Please swipe the card"},
    {14, "or select Manual Entry"},
    {14, "PreAuthorizing"},
    {14, "Authorizing"},
    {14, "Voiding"},
    {14, "Refunding"},
    {14, "Cancelling Refund"},
    {14, "Please Swipe Card"},
    {14, "Please Wait"},

    {15, "Check"},
    {15, "Checks"},
    {15, "All Cash & Checks"},
    {15, "Total Check Payments"},
    {15, "Pre-Auth Complete"},
    {15, "Fast Food"},

    {-1, NULL}
};

/*********************************************************************
 * PhraseInfo Class
 ********************************************************************/

PhraseInfo::PhraseInfo()
{
    FnTrace("PhraseInfo::PhraseInfo()");
    next = NULL;
    fore = NULL;
}

PhraseInfo::PhraseInfo(const char* k, const genericChar* v)
{
    FnTrace("PhraseInfo::PhraseInfo(const char* , const char* )");
    next = NULL;
    fore = NULL;
    key.Set(k);
    value.Set(v);
}

int PhraseInfo::Read(InputDataFile &df, int version)
{
    FnTrace("PhraseInfo::Read()");
    int error = 0;
    error += df.Read(key);
    error += df.Read(value);
    return error;
}

int PhraseInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("PhraseInfo::Write()");
    int error = 0;
    error += df.Write(key);
    error += df.Write(value);
    return error;
}


/*********************************************************************
 * Locale Class
 ********************************************************************/
Locale::Locale()
{
    FnTrace("Locale::Locale()");
    next = NULL;
    fore = NULL;
    search_array = NULL;
    array_size = 0;
}

int Locale::Load(const char* file)
{
    FnTrace("Locale::Load()");
    if (file)
        filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(filename.Value(), version))
        return 1;

    // VERSION NOTES
    // 1 (5/17/97) Initial version

    genericChar str[256];
    if (version < 1 || version > 1)
    {
        vt_safe_string::safe_format(str, 256, "Unknown locale file version %d", version);
        ReportError(str);
        return 1;
    }

    int tmp;
    Purge();
    df.Read(name);
    df.Read(tmp);
    df.Read(tmp);
    df.Read(tmp);
    df.Read(tmp);

    int count = 0;
    df.Read(count);
    for (int i = 0; i < count; ++i)
    {
        PhraseInfo *ph = new PhraseInfo;
        ph->Read(df, 1);
        Add(ph);
    }
    return 0;
}

int Locale::Save()
{
    FnTrace("Locale::Save()");
    if (filename.size() <= 0)
        return 1;

    BackupFile(filename.Value());

    // Save Version 1
    OutputDataFile df;
    if (df.Open(filename.Value(), 1, 1))
        return 1;

    df.Write(name);
    df.Write(0);
    df.Write(0);
    df.Write(0);
    df.Write(0);

    df.Write(PhraseCount());
    for (PhraseInfo *ph = PhraseList(); ph != NULL; ph = ph->next)
        ph->Write(df, 1);
    return 0;
}

int Locale::Add(PhraseInfo *ph)
{
    FnTrace("Locale::Add()");
    if (ph == NULL)
        return 1;

    if (search_array)
    {
	free(search_array);
        search_array = NULL;
        array_size = 0;
    }

    // start at end of list and work backwords
    const genericChar* n = ph->key.Value();
    PhraseInfo *ptr = PhraseListEnd();
    while (ptr && StringCompare(n, ptr->key.Value()) < 0)
        ptr = ptr->fore;

    // Insert ph after ptr
    return phrase_list.AddAfterNode(ptr, ph);
}

int Locale::Remove(PhraseInfo *ph)
{
    FnTrace("Locale::Remove()");
    if (ph == NULL)
        return 1;

    if (search_array)
    {
	free(search_array);
        search_array = NULL;
        array_size = 0;
    }
    return phrase_list.Remove(ph);
}

int Locale::Purge()
{
    FnTrace("Locale::Purge()");
    phrase_list.Purge();

    if (search_array)
    {
	free(search_array);
        search_array = NULL;
        array_size = 0;
    }
    return 0;
}

/****
 * BuildSearchArray: constructs binary search array (other functions
 * call this automatically)
 ****/
int Locale::BuildSearchArray()
{
    FnTrace("Locale::BuildSearchArray()");
    if (search_array)
    {
	free(search_array);
    }

    array_size = PhraseCount();
    search_array = (PhraseInfo **)calloc(sizeof(PhraseInfo *), (array_size + 1));
    if (search_array == NULL)
        return 1;

    PhraseInfo *ph = PhraseList();
    for (int i = 0; i < array_size; ++i)
    {
        search_array[i] = ph;
        ph = ph->next;
    }
    return 0;
}

/****
 * Find: find record for word to translate - returns NULL if none
 ****/
PhraseInfo *Locale::Find(const char* key)
{
    FnTrace("Locale::Find()");
    if (key == NULL)
        return NULL;
    if (search_array == NULL)
        BuildSearchArray();

    int l = 0;
    int r = array_size - 1;
    while (r >= l)
    {
        int x = (l + r) / 2;
        PhraseInfo *ph = search_array[x];

        int val = StringCompare(key, ph->key.Value());
        if (val < 0)
            r = x - 1;
        else if (val > 0)
            l = x + 1;
        else
            return ph;
    }
    return NULL;
}

/****
 * Translate: translates string or just returns original string if no
 * translation
 ****/
const char* Locale::Translate(const char* str, int lang, int clear)
{
    FnTrace("Locale::Translate()");
    char buffer[STRLONG];

    if (lang == LANG_PHRASE)
    {
        PhraseInfo *ph = Find(str);
        if (ph == NULL)
        {
            // If clear flag is set and phrase not found, return empty string
            if (clear)
            {
                static char empty_str[] = "";
                return empty_str;
            }
            return str;
        }
        else
            return ph->value.Value();
    }
    else
    {
        // Check hardcoded translations
        const char* hardcoded = LookupHardcodedTranslation(str, lang);
        if (hardcoded != nullptr) {
            return hardcoded;
        }

        // No translation found, return original string
        return str;
    }

    return str;
}

/****
 * NewTranslation:  adds new translation to database
 ****/
int Locale::NewTranslation(const char* str, const genericChar* value)
{
    FnTrace("Locale::NewTranslation()");
    PhraseInfo *ph = Find(str);
    if (ph)
    {
        ph->value.Set(value);
        if (ph->value.size() > 0)
            return 0;

        Remove(ph);
        delete ph;

        if (search_array)
        {
	    free(search_array);
            search_array = NULL;
            array_size = 0;
        }
        return 0;
    }

    if (value == NULL || strlen(value) <= 0)
        return 1;

    if (search_array)
    {
	free(search_array);
        search_array = NULL;
        array_size = 0;
    }
    return Add(new PhraseInfo(str, value));
}

/****
 * TimeDate: returns time/date nicely formated (format flags are in
 * locale.hh)
 ****/
const char* Locale::TimeDate(Settings *s, const TimeInfo &timevar, int format, int lang, genericChar* str)
{
    FnTrace("Locale::TimeDate()");
	// FIX - implement handler for TD_SECONDS format flag
	// Mon Oct  1 13:14:27 PDT 2001: some work done in this direction - JMK

    static genericChar buffer[256];
    if (str == NULL)
        str = buffer;

    if (!timevar.IsSet())
    {
        vt_safe_string::safe_format(str, 256, "<NOT SET>");
        return str;
    }

    genericChar tempstr[256];
    str[0] = '\0';

    if (!(format & TD_NO_DAY))
    {
        // Show Day of Week
        int wd = timevar.WeekDay();
        if (format & TD_SHORT_DAY)
            vt_safe_string::safe_format(str, 256, "%s", Translate(ShortDayName[wd], lang));
        else
            vt_safe_string::safe_format(str, 256, "%s", Translate(DayName[wd], lang));

        if (!(format & TD_NO_TIME) || !(format & TD_NO_DATE))
            vt_safe_string::safe_concat(str, 256, ", ");
    }

    if (!(format & TD_NO_DATE))
    {
        // Show Date
        int d = timevar.Day();
        int y = timevar.Year();
        int m = timevar.Month();
        if (format & TD_SHORT_DATE)
        {
            if (s->date_format == DATE_DDMMYY)
            {
                int temp = m;
                m = d;
                d = temp;
            }
	
            if (format & TD_PAD)
                vt_safe_string::safe_format(tempstr, 256, "%2d/%2d", m, d);
            else
                vt_safe_string::safe_format(tempstr, 256, "%d/%d", m, d);
            vt_safe_string::safe_concat(str, 256, tempstr);
            if (!(format & TD_NO_YEAR))
            {
                vt_safe_string::safe_format(tempstr, 256, "/%02d", y % 100);
                vt_safe_string::safe_concat(str, 256, tempstr);
            }
        }
        else
        {
            if (format & TD_SHORT_MONTH)
            {
                if (format & TD_MONTH_ONLY)
                    vt_safe_string::safe_format(tempstr, 256, "%s", Translate(ShortMonthName[m - 1], lang));
                else if (format & TD_PAD)
                    vt_safe_string::safe_format(tempstr, 256, "%s %2d", Translate(ShortMonthName[m - 1], lang), d);
                else
                    vt_safe_string::safe_format(tempstr, 256, "%s %d", Translate(ShortMonthName[m - 1], lang), d);
            }
            else
            {
                if (format & TD_MONTH_ONLY)
                    vt_safe_string::safe_format(tempstr, 256, "%s", Translate(MonthName[m - 1], lang));
                else if (format & TD_PAD)
                    vt_safe_string::safe_format(tempstr, 256, "%s %2d", Translate(MonthName[m - 1], lang), d);
                else
                    vt_safe_string::safe_format(tempstr, 256, "%s %d", Translate(MonthName[m - 1], lang), d);
            }
            vt_safe_string::safe_concat(str, 256, tempstr);

            if (!(format & TD_NO_YEAR))
            {
                vt_safe_string::safe_format(tempstr, 256, ", %d", y);
                vt_safe_string::safe_concat(str, 256, tempstr);
            }
        }

        if (! (format & TD_NO_TIME))
            vt_safe_string::safe_concat(str, 256, " - ");
    }

    if (! (format & TD_NO_TIME))
    {
        int hr = timevar.Hour();
        int minute = timevar.Min();
        int sec = timevar.Sec();
        int pm = 0;

        if (hr >= 12)
            pm = 1;
        hr = hr % 12;
        if (hr == 0)
            hr = 12;

        if (format & TD_PAD)
        {
            if (format & TD_SHORT_TIME)
			{
				if(format & TD_SECONDS)
					vt_safe_string::safe_format(tempstr, 256, "%2d:%02d:%2d%c", hr, minute, sec, AMorPM[pm][0]);
				else
					vt_safe_string::safe_format(tempstr, 256, "%2d:%02d%c", hr, minute, AMorPM[pm][0]);
			}
            else
			{
				if(format & TD_SECONDS)
					vt_safe_string::safe_format(tempstr, 256, "%2d:%02d:%2d %s", hr, minute, sec, AMorPM[pm]);
				else
					vt_safe_string::safe_format(tempstr, 256, "%2d:%02d %s", hr, minute, AMorPM[pm]);
			}
        }
        else
        {
            if (format & TD_SHORT_TIME)
			{
				if(format & TD_SECONDS)
					vt_safe_string::safe_format(tempstr, 256, "%2d:%02d:%2d%c", hr, minute, sec, AMorPM[pm][0]);
				else
					vt_safe_string::safe_format(tempstr, 256, "%d:%02d%c", hr, minute, AMorPM[pm][0]);
			}
            else
			{
				if(format & TD_SECONDS)
					vt_safe_string::safe_format(tempstr, 256, "%2d:%02d:%2d %s", hr, minute, sec, AMorPM[pm]);
				else
					vt_safe_string::safe_format(tempstr, 256, "%d:%02d %s", hr, minute, AMorPM[pm]);
			}
        }
        vt_safe_string::safe_concat(str, 256, tempstr);
    }

    return str;
}

/****
 * Page:  returns nicely formated & translated page numbering
 ****/
char* Locale::Page(int current, int page_max, int lang, genericChar* str)
{
    FnTrace("Locale::Page()");
    static genericChar buffer[32];
    if (str == NULL)
        str = buffer;

    // Ensure current and page_max are valid
    if (current < 1)
        current = 1;
    if (page_max < 0)
        page_max = 0;

    if (page_max <= 0)
        vt_safe_string::safe_format(str, 32, "%s %d", Translate("Page", lang), current);
    else
        vt_safe_string::safe_format(str, 32, "%s %d %s %d", Translate("Page", lang), current,
                Translate("of", lang), page_max);
    return str;
}
