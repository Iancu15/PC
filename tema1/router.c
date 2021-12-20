#include "include/queue.h"
#include "include/skel.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <inttypes.h>

uint16_t ip_checksum(void* vdata,size_t length);

void build_ethhdr(struct ether_header *eth_hdr, uint8_t *sha, uint8_t *dha, unsigned short type);

/**
 * Pentru un ip primit la intrare, daca gaseste o interfata al carei ip este la fel
 * va intoarce prin intermediul variabilei router_interface index-ul interfetei
 */
void get_ip_interface(uint32_t ip, int number_of_interfaces, int* router_interface) {
	for (int interface_index = 0; interface_index < number_of_interfaces; interface_index++) {
		struct in_addr interfaceAddr;
		inet_aton(get_interface_ip(interface_index), &interfaceAddr);
		if (inet_addr(get_interface_ip(interface_index)) == ip) {
			*router_interface = interface_index;
			break;
		}
	}
}

/**
 * Structura unei intrari din tabela de rutare
 */
typedef struct entry {
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
} Entry;

/**
 * Structura tabelei de rutare, campul masks reprezinta lista de masti
 * folosite de intrarile din tabela
 */
typedef struct route_table {
	Entry** table;
	int size;
	uint32_t* masks;
	int number_of_masks;
} Route_table;

/**
 * Citeste tabela de rutare dintr-un fisier al carui path e dat ca parametru
 */
Route_table* read_route_table(char* filename) {
	FILE* fp = fopen(filename, "r");
	DIE(!fp, "open_route_table");
	char line[50];
	Route_table* route_table = (Route_table*) malloc(sizeof(Route_table));
	route_table->table = (Entry**) malloc(10 * sizeof(Entry*));

	// masca difera doar prin numarul de biti setati, cum masca e pe 32 biti,
	// atunci nu pot fi mai mult de 32 de tipuri de masti diferite
	route_table->masks = (uint32_t*) malloc(32 * sizeof(uint32_t));
	route_table->number_of_masks = 0;
	DIE(!route_table, "alloc_route_table");
	int entry_index = 0;
	int route_table_size = 10;

	// pentru fiecare linie din fisier creez intrarea corespondenta in tabela de rutare
	// folosesc inet_addr sa transform sirurile de caractere in reprezentarea uint32_t
	while(fgets(line, 50, fp)) {
		if (entry_index == route_table_size) {
			route_table_size *= 2;
			route_table->table = realloc(route_table->table, route_table_size * sizeof(Entry));
			DIE(!route_table, "realloc_route_table");
		}

		route_table->table[entry_index] = (Entry*)malloc(sizeof(Entry));
		char* token = strtok(line, " ");
		route_table->table[entry_index]->prefix = inet_addr(token);
		token = strtok(NULL, " ");
		route_table->table[entry_index]->next_hop = inet_addr(token);
		token = strtok(NULL, " ");
		route_table->table[entry_index]->mask = inet_addr(token);
		token = strtok(NULL, " ");
		route_table->table[entry_index]->interface = atoi(token);
		int maskAlreadyAdded = 0;
		for (int i = 0; i < route_table->number_of_masks; i++) {
			if (route_table->masks[i] == route_table->table[entry_index]->mask) {
				maskAlreadyAdded = 1;
			}
		}

		// daca masca nu a fost gasita la nici o alta intrare in tabela de rutare, 
		// atunci o adaug la lista de masti
		if (!maskAlreadyAdded) {
			uint32_t mask = route_table->table[entry_index]->mask;
			route_table->masks[route_table->number_of_masks] = mask;
			route_table->number_of_masks += 1;
		}

		entry_index++;
	}

	fclose(fp);
	route_table->size = entry_index;
	return route_table;
}

/**
 * Cauta in tabela de rutare daca exista un prefix egal cu ip & mask
 * folosind cautarea binara, tabela de rutare e sortata in prealabil dupa prefix
 */
Entry* binary_search_table(Route_table* route_table, uint32_t ip, uint32_t mask) {
	int left = 0;
	int right = route_table->size - 1;

	Entry* searched_entry = NULL;
	uint32_t ip_prefix = ip & mask;
	while (left <= right) {
		int mid = (left + right) / 2;
		Entry* entry = route_table->table[mid];;
		if (ip_prefix == entry->prefix) {
			searched_entry = entry;
			break;
		}

		if (ip_prefix < entry->prefix) {
			right = mid - 1;
		} else {
			left = mid + 1;
		}
	}

	return searched_entry;
}

/**
 * Se cauta in tabela de rutare folosind functia de mai sus daca exista
 * vreun prefix pentru orice tip de combinatie ip & mask, unde mask
 * ia valori din lista de masti
 * se considera lista de masti sortata in prealabil descrescator, astfel
 * incat sa se testeze de la cea mai mare masca la cea mica si implicit
 * de la cel mai restrictiv prefix la cel mai putin restrictiv
 */
Entry* search_route_table(Route_table* route_table, uint32_t ip) {
	Entry* searched_entry = NULL;
	for (int maskIndex = 0; maskIndex < route_table->number_of_masks; maskIndex++) {
		searched_entry = binary_search_table(route_table, ip, route_table->masks[maskIndex]);
		if (searched_entry != NULL) {
			break;
		}
	}

	return searched_entry;
}

/**
 * Structura unei intrari in tabela ARP
 */
typedef struct entryARP {
	uint32_t ip;
	uint8_t* mac;
} ARPEntry;

/**
 * Structura tabelei ARP
 */
typedef struct arp_table {
	ARPEntry** table;
	int size;
	int capacity;
} ARPtable;

/**
 * Functie care creeaza o tabela ARP goala alocand spatiu pentru
 * un numar de intrari egale cu capacitatea specificata la intrare
 */
ARPtable* create_ARP_table(int capacity) {
	ARPtable* arp_table = (ARPtable*) malloc(sizeof(ARPtable));
	DIE(!arp_table, "alloc_arp_table");
	arp_table->table = (ARPEntry**) malloc(capacity * sizeof(ARPEntry*));
	DIE(!arp_table->table, "alloc_arp_table_table");
	arp_table->capacity = capacity;
	arp_table->size = 0;

	return arp_table;
}

/**
 * Cauta in tabela ARP mac-ul corespondent ip-ului dat la intrare
 * daca ip-ul nu se afla in tabela ARP intoarce NULL
 */
uint8_t* search_arp_mac(ARPtable* arp_table, uint32_t ip) {
	uint8_t* searched_mac = NULL;
	for (int entry_index = 0; entry_index < arp_table->size; entry_index++) {
		if (arp_table->table[entry_index]->ip == ip) {
			searched_mac = arp_table->table[entry_index]->mac;
			break;
		}
	}

	return searched_mac;
}

/**
 * Adauga o intrare in tabela ARP constituita din ip-ul si mac-ul primit
 * la intrare
 */
void add_ARP_entry(ARPtable* arp_table, uint32_t ip, uint8_t* mac) {
	// daca intrarea este deja prezenta se iese din functie
	if (search_arp_mac(arp_table, ip) != NULL) {
		return;
	}

	arp_table->table[arp_table->size] = (ARPEntry*) malloc(sizeof(ARPEntry));
	ARPEntry* arp_entry = arp_table->table[arp_table->size];
	DIE(!arp_entry, "alloc_arp_entry");
	arp_entry->ip = ip;

	// cum mac-ul e camp de tip pointer in loc de array de 6, el
	// trebuie alocat dinamic si valoarea copiata cu memcpy in acesta
	arp_entry->mac = malloc(sizeof(uint8_t) * 6);
	memcpy(arp_entry->mac, mac, sizeof(uint8_t) * 6);
	arp_table->size++;
	if (arp_table->capacity == arp_table->size) {
		arp_table->capacity *= 2;
		arp_table->table = realloc(arp_table->table, arp_table->capacity * sizeof(ARPEntry*));
		DIE(!arp_table->table, "realloc_arp_table_table");
	}
}

/**
 * Structura unei intrari din coada de asteptare a pachetelor pentru care nu era
 * cunoscut mac-ul next hop-ului
 * aceasta contine pachetul ce va trebui dirijat intr-un punct ulterior dupa ce
 * va fi primit ARP Reply-ul cu mac-ul aferent,
 * ip-ul next hop-ului unde trebuie dirijat si interfata pe care trebuie trimis
 */
typedef struct arp_queue_entry {
	packet* m;
	uint32_t next_hop;
	int interface;
} QueueEntry;

/**
 * Functie de calculare al ip-ului folosind algoritmul incremental din RFC 1624
 * aceasta primeste checksum-ul si ttl-ul dinainte decrementarea acestuia si
 * calculeaza noul checksum folosind ecuatia numarul 3 din RFC 1624
 */
uint16_t ip_checksum_rfc_1624(uint16_t old_checksum, uint8_t old_ttl) {
	uint8_t new_ttl = old_ttl - 1;

	// din experimentare am obversat ca ecuatia 3 intoarce o valoare cu 1 mai mare
	// decat cea intoarsa de ip_checksum, asa ca am modificat corespunzator
	uint16_t new_checksum = ~(~old_checksum + ~old_ttl + new_ttl) - 1;

	// checksum-ul n-ar trebui sa fie 0xffff asa ca intorc ~0xffff = 0x0000
	if(new_checksum == (uint16_t)0xffff) {
		return 0;
	}

	return new_checksum;
}

/**
 * Compara 2 intrari din tabela rutare intr-un sens crescator
 * folosit de qsort
 */
int compareEntries(const void* a, const void* b) {
	return (*(Entry**)a)->prefix - (*(Entry**)b)->prefix;
}

/**
 * Compara 2 masti intr-un sens descrescator, sensul e descrescator
 * folosit de qsort pe lista de masti din route_table
 */
int compareMasks(const void* a, const void* b) {
	uint32_t amask = *(uint32_t*)a;
	uint32_t bmask = *(uint32_t*)b;

	// 255.255.255.255 este -1 in uint32_t asa ca trebuie luat
	// separat, restul mastilor sunt numere naturale
	if (amask == -1) {
		return -1;
	} else if (bmask == -1) {
		return 1;
	}

	// sensul e descrescator
	return (bmask - amask);
}

int main(int argc, char *argv[])
{
	packet m;
	int rc;
	init(argc - 2, argv + 2);

	// aloc dinamic pentru ca pe stiva este rescris de variabilele statice din while
	int* number_of_interfaces = malloc(sizeof(int));
	*number_of_interfaces = argc - 2;

	/**
	 * Citesc tabela de rutera din fisier si o sortez crescator dupa prefix pentru a
	 * putea cauta cu cautarea binara in el si sortez lista de masti descrescator pentru
	 * a cauta de la cel mai restrictiv la cel mai putin restrictiv prefix
	 */
	Route_table* route_table = read_route_table(argv[1]);
	qsort(route_table->table, route_table->size, sizeof(Entry*), compareEntries);
	DIE(route_table->size == 0 || route_table->number_of_masks == 0, "read_route_table_size");
	qsort(route_table->masks, route_table->number_of_masks, sizeof(uint32_t), compareMasks);
	ARPtable* arp_table = create_ARP_table(10);
	queue q = queue_create();
	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");
		int packet_interface = (&m)->interface;

		// router_mac il aloc dinamic pentru a nu fi rescris de alte variabile statice
		uint8_t *router_mac = malloc(6 * sizeof(uint8_t));
		uint32_t router_ip = inet_addr(get_interface_ip(packet_interface));
		get_interface_mac(packet_interface, router_mac);
		char* packet_payload = (&m)->payload;
		struct ether_header *eth_hdr = (struct ether_header*)packet_payload;
		int IP_OFFSET = sizeof(struct ether_header);
		struct arp_header *arp_hdr = parse_arp(packet_payload);
		struct iphdr *ip_hdr = (struct iphdr*)(packet_payload + IP_OFFSET);
		struct icmphdr *icmp_hdr = parse_icmp(packet_payload);

		// daca arp_hdr e diferit de NULL inseamna ca pachetul este de tip ARP
		if (arp_hdr != NULL) {

			/**
			 * Daca pachetul este de tip ARP_REQUEST si este destinat catre unul dintre
			 * interfetele routerului, atunci obtin mac-ul interfetei si il pun ca sursa
			 * la creearea header-ului ethernet, destinatia fiind mac-ul retelei care mi-a
			 * trimis arp request
			 * 
			 * Initializez interfata arp inainte sa o caut cu o valoarea negativa(-5)
			 * pentru ca stiu ca index-ul interfetelor este >= 0
			 * */
			int arp_interface = -5;
			get_ip_interface(arp_hdr->tpa, *number_of_interfaces, &arp_interface);
			if (ntohs(arp_hdr->op) == ARPOP_REQUEST && arp_interface != -5) {
				struct ether_header reply_eth_hdr;
				uint8_t reply_router_mac;
				get_interface_mac(arp_interface, &reply_router_mac);
				build_ethhdr(&reply_eth_hdr, &reply_router_mac,
													eth_hdr->ether_shost, htons(ETHERTYPE_ARP));
				send_arp(arp_hdr->spa, arp_hdr->tpa, &reply_eth_hdr,
														 packet_interface, htons(ARPOP_REPLY));
				continue;
			}

			/**
			 * Daca pachetul este de tip ARP_REPLY, atunci adaug o intrare in
			 * tabela ARP cu ip-ul si mac-ul sursa din pachet
			 * 
			 * Daca dupa ce s-a adaugat intrarea sunt pachete in coada ce trebuie
			 * dirijate, atunci se interogheaza tabela ARP pentru next hop-ul pe
			 * care trebuie trimis pachetul si se pune mac-ul acestuia ca destinatie
			 * pentru header-ul ethernet, pachetul este apoi trimis pe interfata
			 * corespondenta next hop-ului prezenta in structura queue_entry
			 */
			if (ntohs(arp_hdr->op) == ARPOP_REPLY) {
				add_ARP_entry(arp_table, arp_hdr->spa, arp_hdr->sha);
				while (!queue_empty(q)) {
					QueueEntry *queue_entry = queue_deq(q);
					uint8_t *destination_mac = search_arp_mac(arp_table, queue_entry->next_hop);
					DIE(!destination_mac, "arp_reply_dest_mac_not_found");

					packet* mpacket = queue_entry->m;
					struct ether_header *forward_eth_hdr = (struct ether_header*)mpacket->payload;
					memcpy(forward_eth_hdr->ether_dhost, destination_mac, sizeof(uint8_t) * 6);
					int sent = send_packet(queue_entry->interface, mpacket);
					free(queue_entry);
					DIE(!sent, "queue_packet_not_sent");

					// Daca s-a gasit fie acum, fie intr-un punct ulterior mac-ul urmatorului
					// pachet din coada, atunci il dirijez si pe el, fac asta
					// pana cand se ajunge la un pachet in coada pentru care nu stim mac-ul
					// next hop-ului
					QueueEntry* top_entry = queue_top(q);
					if (top_entry != NULL && search_arp_mac(arp_table, top_entry->next_hop) != NULL) {
						continue;
					}

					break;
				}

				continue;
			}

			// se ajunge aici in cazul in care se primeste un pachet ARP_REQUEST care
			// nu este destinat router-ului, pachetul este aruncat
			continue;
		}

		/**
		 * Daca pachetul este destinat router-ului(si nu e ARP_Reply, cazul fiind abordat mai sus),
		 * iar acesta este de tip ICMP_REPLY(header-ul parsat este diferit de NULL si are cod 0
		 * si tip 8), atunci se trimite un ICMP_REPLY catre sursa pachetului printr-o
		 * interschimbare a mac-urilor si ip-urilor sursa <-> destinatie ale pachetului primit
		 */
		int router_interface = -5;
		get_ip_interface(ip_hdr->daddr, *number_of_interfaces, &router_interface);
		if (router_interface != -5) {
			if (icmp_hdr != NULL && (icmp_hdr->code == 0 && icmp_hdr->type == 8)) {
				send_icmp(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost, eth_hdr->ether_shost,
						0, 0, packet_interface, icmp_hdr->un.echo.id, icmp_hdr->un.echo.sequence);
				continue;
			}

			// se ajunge aici daca pachetul este destinat router-ului si nu este ICMP_REQUEST, in
			// acel caz pachetul este aruncat
			continue;
		}

		/**
		 * Daca time to live-ul este mai mic sau egal cu 1 inseamna ca pachetul va expira in
		 * mainile router-ului si astfel se trimite un mesaj de icmp error catre sursa de
		 * unde a venit pachetul cu tipul 11 si codul 0 -> Time Exceeded
		 */
		if (ip_hdr->ttl <= 1) {
			send_icmp_error(ip_hdr->saddr, router_ip, router_mac, 
										eth_hdr->ether_shost, 11, 0, packet_interface);
			continue;
		}

		/**
		 * Daca checksum-ul este gresit se arunca pachetul
		 */
		if (ip_checksum(ip_hdr, sizeof(struct iphdr)) != 0) {
			continue;
		}

		/**
		 * Se calculeaza noul checksum folosind algoritmul RFC 1624 si se
		 * decrementeaza ttl-ul
		 */
		uint16_t new_check = ip_checksum_rfc_1624(ip_hdr->check, ip_hdr->ttl);
		ip_hdr->ttl--;
		ip_hdr->check = new_check;

		/**
		 * Daca destinatia pachetului nu a fost gasita in tabela de rutare, atunci se
		 * trimite un mesaj de eroare ICMP cu tipul 3 si codul 0 -> Destination Unreachable
		 */
		Entry *destination_entry = search_route_table(route_table, ip_hdr->daddr);
		if (destination_entry == NULL) {
			send_icmp_error(ip_hdr->saddr, router_ip, router_mac, 
										eth_hdr->ether_shost, 3, 0, packet_interface);
			continue;
		}

		/**
		 * Se iau informatiile necesare cu informatiile din intrarea tabelei de rutare:
		 * next hop-ul, interfata pe care trebui trimis pachetul si mac-ul interfetei
		 */
		free(router_mac);
		int drouter_interface = destination_entry->interface;
		uint32_t dnext_hop = destination_entry->next_hop;
		uint8_t drouter_mac;
		get_interface_mac(drouter_interface, &drouter_mac);

		/**
		 * Se seteaza in header-ul ethernet sursa ca fiind interfata pe care urmeaza
		 * sa fie trimis pachetul, apoi se cauta in tabela ARP mac-ul urmatorului hop,
		 * daca acesta nu a fost gasit se trimite un ARP_REQUEST ce are drept
		 * destinatie mac-ul broadcast
		 */
		memcpy(eth_hdr->ether_shost, &drouter_mac, sizeof(uint8_t) * 6);
		uint8_t *destination_mac = search_arp_mac(arp_table, dnext_hop);
		if (destination_mac == NULL) {
			uint8_t broadcast_mac;
			hwaddr_aton("ff:ff:ff:ff:ff:ff", &broadcast_mac);
			struct ether_header arp_request_eth_hdr;
			build_ethhdr(&arp_request_eth_hdr, eth_hdr->ether_dhost, &broadcast_mac, htons(ETHERTYPE_ARP));
			send_arp(dnext_hop, inet_addr(get_interface_ip(drouter_interface)), 
									&arp_request_eth_hdr, drouter_interface, htons(ARPOP_REQUEST));

			// Pachetul ce nu poate fi dirijat momentan este bagat in coada alaturi de ip-ul
			// next hop-ului si interfata pe care urma sa fie trimis pachetul
			QueueEntry* queue_entry = (QueueEntry*) malloc(sizeof(QueueEntry));
			queue_entry->m = (packet*)malloc(sizeof(packet));
			memcpy(queue_entry->m, &m, sizeof(packet));
			queue_entry->next_hop = dnext_hop;
			queue_entry->interface = drouter_interface;
			queue_enq(q, queue_entry);
			continue;
		}

		// Daca de fapt a fost gasit mac-ul next hop-ului, atunci se seteaza drept
		// destinatie in header-ul ethernet si se trimite pachetul pe interfata
		// corespondenta urmatorului hop
		memcpy(eth_hdr->ether_dhost, destination_mac, sizeof(uint8_t) * 6);
		send_packet(drouter_interface, &m);
	}
}