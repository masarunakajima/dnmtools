/*    methcounts: a program for counting the methylated and
 *    unmethylated reads mapping over each CpG or C
 *
 *    Copyright (C) 2009 University of Southern California and
 *                       Andrew D. Smith
 *
 *    Authors: Andrew D. Smith
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <vector>
#include <iostream>
#include <iterator>
#include <fstream>
#include <algorithm>
#include <numeric>

#include <tr1/unordered_map>

#include "OptionParser.hpp"
#include "rmap_utils.hpp"
#include "rmap_os.hpp"
#include "GenomicRegion.hpp"
#include "QualityScore.hpp"

#include "bsutils.hpp"
#include "FileIterator.hpp"

using std::tr1::unordered_map;

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
using std::max;

static void
add_contribution_cpg(const size_t offset, const GenomicRegion &r,
		     const string &s, size_t &meth, size_t &unmeth) {
  if (r.pos_strand() && (r.get_start() <= offset)) {
    const size_t position = offset - r.get_start();
    if (position < s.length()) {
      assert(position < s.length());
      if (is_cytosine(s[position])) ++meth;
      if (is_thymine(s[position])) ++unmeth;
    }
  }
  else if (offset - r.get_start() + 2 <= s.length()) {
    const size_t position = (s.length() - 1) - 
      ((offset + 1) - r.get_start());
    if (position < s.length()) {
      assert(position < s.length());
      if (is_cytosine(s[position])) ++meth;
      if (is_thymine(s[position])) ++unmeth;
    }
  }
}


static void
add_contribution_c(const size_t offset, const GenomicRegion &r,
		   const string &s, size_t &meth, size_t &unmeth) {
  if (r.pos_strand()) {
    const size_t position = offset - r.get_start();
    assert(position < s.length());
    if (is_cytosine(s[position])) ++meth;
    if (is_thymine(s[position])) ++unmeth;
  }
}


static void
add_contribution_g(const size_t offset, const GenomicRegion &r,
		   const string &s, size_t &meth, size_t &unmeth) {
  if (r.neg_strand()) {
    const size_t position = s.length() - (offset - r.get_start() + 1);
    assert(position < s.length());
    if (is_cytosine(s[position])) ++meth;
    if (is_thymine(s[position])) ++unmeth;
  }
}

/***********************************************************************
 * FUNCTIONS BELOW ARE FOR FASTQRecord OJBECTS AND *NOT* USING THE
 * QUALITY INFORAMTION
 */
static void
add_contribution_cpg(const size_t offset, const GenomicRegion &r,
		     const FASTQRecord &s, size_t &meth, size_t &unmeth) {
  if (r.pos_strand() && 
      // (offset - r.get_start() < s.first.length()) &&
      (r.get_start() <= offset)) {
    const size_t position = offset - r.get_start();
    if (!(position < s.first.length())) {
      cerr << position << endl
	   << s.first << endl
	   << r << endl;
      assert(position < s.first.length());
    }
    if (is_cytosine(s.first[position])) ++meth;
    if (is_thymine(s.first[position])) ++unmeth;
  }
  if (r.neg_strand() && 
      // (r.get_start() <= offset + 1) && 
      (offset - r.get_start() + 2 <= s.first.length())) {
    const size_t position = (s.first.length() - 1) - 
      ((offset + 1) - r.get_start());
    assert(position < s.first.length());
    if (is_cytosine(s.first[position])) ++meth;
    if (is_thymine(s.first[position])) ++unmeth;
  }
}

static void
add_contribution_c(const size_t offset, const GenomicRegion &r,
		   const FASTQRecord &s, size_t &meth, size_t &unmeth) {
  if (r.pos_strand()) {
    const size_t position = offset - r.get_start();
    assert(position < s.first.length());
    if (is_cytosine(s.first[position])) ++meth;
    if (is_thymine(s.first[position])) ++unmeth;
  }
}

static void
add_contribution_g(const size_t offset, const GenomicRegion &r,
		   const FASTQRecord &s, size_t &meth, size_t &unmeth) {
  if (r.neg_strand()) {
    const size_t position = (s.first.length() - 1) - (offset - r.get_start());
    assert(position < s.first.length());
    if (is_cytosine(s.first[position])) ++meth;
    if (is_thymine(s.first[position])) ++unmeth;
  }
}


/***********************************************************************
 * FUNCTIONS BELOW ARE FOR FASTQRecord OJBECTS AND USING THE QUALITY
 * INFORAMTION
 */

struct QualityChecker {
  QualityChecker(const FASTQScoreType &score_format, const double c) {
    cutoff = (FASTQScoreIsPhred(score_format) ?
	      phred_to_quality_character(error_probability_to_phred(c)) :
	      solexa_to_quality_character(error_probability_to_solexa(c)));
  }
  bool operator()(const FASTQRecord &s, const size_t position) const {
    return s.second[position] >= cutoff;
  }
  char cutoff;
};

static void
add_contribution_cpg(const QualityChecker &qc,
		     const size_t offset, const GenomicRegion &r,
		     const FASTQRecord &s, size_t &meth, size_t &unmeth) {
  if (r.pos_strand() && (r.get_start() <= offset)) {
    const size_t position = offset - r.get_start();
    assert(position < s.first.length());
    if (qc(s, position)) {
      if (is_cytosine(s.first[position])) ++meth;
      if (is_thymine(s.first[position])) ++unmeth;
    }
  }
  if (r.neg_strand() && (offset - r.get_start() + 2 <= s.first.length())) {
    const size_t position = (s.first.length() - 1) - 
      ((offset + 1) - r.get_start());
    assert(position < s.first.length());
    if (qc(s, position)) {
      if (is_cytosine(s.first[position])) ++meth;
      if (is_thymine(s.first[position])) ++unmeth;
    }
  }
}

static void
add_contribution_c(const QualityChecker &qc,
		   const size_t offset, const GenomicRegion &r,
		   const FASTQRecord &s, size_t &meth, size_t &unmeth) {
  if (r.pos_strand()) {
    const size_t position = offset - r.get_start();
    assert(position < s.first.length());
    if (qc(s, position)) {
      if (is_cytosine(s.first[position])) ++meth;
      if (is_thymine(s.first[position])) ++unmeth;
    }
  }
}

static void
add_contribution_g(const QualityChecker &qc,
		   const size_t offset, const GenomicRegion &r,
		   const FASTQRecord &s, size_t &meth, size_t &unmeth) {
  if (r.neg_strand()) {
    const size_t position = (s.first.length() - 1) - (offset - r.get_start());
    assert(position < s.first.length());
    if (qc(s, position)) {
      if (is_cytosine(s.first[position])) ++meth;
      if (is_thymine(s.first[position])) ++unmeth;
    }
  }
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

static bool
precedes(const GenomicRegion &r, const size_t offset) {
  return r.get_end() <= offset;
}


static bool
succeeds(const GenomicRegion &r, const size_t offset) {
  return r.get_start() > offset;
}


template <class T> void
advance(const size_t first, const size_t last,
	const GenomicRegion &chrom_region, 
	FileIterator<GenomicRegion> &regions,
	FileIterator<T> &reads) {
  while (regions.last_is_good() && 
	 chrom_region.same_chrom(*regions.get_last()) &&
	 !succeeds(*regions.get_last(), last)) {
    regions.increment_last();
    reads.increment_last();
  }
  //   if (regions.last_is_good() != reads.last_is_good())
  //     throw RMAPException("read and map files seem out of sync");
  while (regions.first_is_good() && 
	 chrom_region.same_chrom(*regions.get_first()) &&
	 precedes(*regions.get_first(), first)) {
    regions.increment_first();
    reads.increment_first();
  }
  //   if (regions.first_is_good() != reads.first_is_good())
  //     throw RMAPException("read and map files seem out of sync");
}


template <class T> void
scan_chromosome_cpg(const QualityChecker &qc,
		    const string &chrom,
		    const GenomicRegion &chrom_region,
		    const double max_mismatches,
		    FileIterator<GenomicRegion> &regions, 
		    FileIterator<T> &reads,
		    std::ostream &out) {
  const string chrom_name(chrom_region.get_chrom());
  for (size_t i = 0; i < chrom.length() - 1 && regions.first_is_good(); ++i) {
    if (is_cpg(chrom, i)) {
      /* need the "+1" below because of the 'G' in CpG */
      advance(i, i + 1, chrom_region, regions, reads);
      size_t meth_count = 0, unmeth_count = 0;
      typename vector<T>::const_iterator k(reads.get_first());
      for (vector<GenomicRegion>::const_iterator j(regions.get_first());
	   j != regions.get_last(); ++j, ++k)
	if (j->get_score() <= max_mismatches)
	  add_contribution_cpg(qc, i, *j, *k, meth_count, unmeth_count);
      const double total = meth_count + unmeth_count;
      out << chrom_name << "\t" << i << "\t" << i + 1 << "\tCpG:" 
	  << total << "\t" << meth_count/max(1.0, total) << "\t+\n";
    }
  }
}


template <class T> void
scan_chromosome(const QualityChecker &qc,
		const string &chrom, const GenomicRegion &chrom_region,
		const double max_mismatches,
		FileIterator<GenomicRegion> &regions, 
		FileIterator<T> &reads,
		std::ostream &out) {
  const string chrom_name(chrom_region.get_chrom());
  
  for (size_t i = 0; i < chrom.length() - 1 && regions.first_is_good(); ++i) {
    advance(i, i, chrom_region, regions, reads);
    if (is_cytosine(chrom[i])) { // && !is_guanine(chrom[i + 1])) {
      size_t meth_count = 0, unmeth_count = 0;
      typename vector<T>::const_iterator k(reads.get_first());
      for (vector<GenomicRegion>::const_iterator j(regions.get_first());
	   j != regions.get_last(); ++j, ++k)
	if (j->get_score() <= max_mismatches)
	  add_contribution_c(qc, i, *j, *k, meth_count, unmeth_count);
      const double total = meth_count + unmeth_count;
      out << chrom_name << "\t" << i << "\t" << i + 1 << "\tC:"
	  << total << "\t" << meth_count/max(1.0, total) << "\t+\n";
    }
    if (is_guanine(chrom[i])) { // && !is_cytosine(chrom[i - 1])) {
      size_t meth_count = 0, unmeth_count = 0;
      typename vector<T>::const_iterator k(reads.get_first());
      for (vector<GenomicRegion>::const_iterator j(regions.get_first());
	   j != regions.get_last(); ++j, ++k)
	if (j->get_score() <= max_mismatches)
	  add_contribution_g(qc, i, *j, *k, meth_count, unmeth_count);
      const double total = meth_count + unmeth_count;
      out << chrom_name << "\t" << i << "\t" << i + 1 << "\tG:" 
	  << total << "\t" << meth_count/max(1.0, total) << "\t+\n";
    }
  }
}


template <class T> void
scan_chromosome_cpg(const string &chrom,
		    const GenomicRegion &chrom_region,
		    const double max_mismatches,
		    FileIterator<GenomicRegion> &regions, 
		    FileIterator<T> &reads,
		    std::ostream &out) {
  const string chrom_name(chrom_region.get_chrom());
  for (size_t i = 0; i < chrom.length() - 1 && regions.first_is_good(); ++i) {
    if (is_cpg(chrom, i)) {
      /* need the "+1" below because of the 'G' in CpG */
      advance(i, i + 1, chrom_region, regions, reads);
      size_t meth_count = 0, unmeth_count = 0;
      typename vector<T>::const_iterator k(reads.get_first());
      for (vector<GenomicRegion>::const_iterator j(regions.get_first());
	   j != regions.get_last(); ++j, ++k)
	if (j->get_score() <= max_mismatches)
	  add_contribution_cpg(i, *j, *k, meth_count, unmeth_count);
      const double total = meth_count + unmeth_count;
      out << chrom_name << "\t" << i << "\t" << i + 1 << "\tCpG:" 
	  << total << "\t" << meth_count/max(1.0, total) << "\t+\n";
    }
  }
}


template <class T> void
scan_chromosome(const string &chrom, const GenomicRegion &chrom_region,
		const double max_mismatches,
		FileIterator<GenomicRegion> &regions, 
		FileIterator<T> &reads,
		std::ostream &out) {
  const string chrom_name(chrom_region.get_chrom());
  
  for (size_t i = 0; i < chrom.length() - 1 && regions.first_is_good(); ++i) {
    advance(i, i, chrom_region, regions, reads);
    if (is_cytosine(chrom[i])) { // && !is_guanine(chrom[i + 1])) {
      size_t meth_count = 0, unmeth_count = 0;
      typename vector<T>::const_iterator k(reads.get_first());
      for (vector<GenomicRegion>::const_iterator j(regions.get_first());
	   j != regions.get_last(); ++j, ++k)
	if (j->get_score() <= max_mismatches)
	  add_contribution_c(i, *j, *k, meth_count, unmeth_count);
      const double total = meth_count + unmeth_count;
      out << chrom_name << "\t" << i << "\t" << i + 1 << "\tC:"
	  << total << "\t" << meth_count/max(1.0, total) << "\t+\n";
    }
    if (is_guanine(chrom[i])) { // && !is_cytosine(chrom[i - 1])) {
      size_t meth_count = 0, unmeth_count = 0;
      typename vector<T>::const_iterator k(reads.get_first());
      for (vector<GenomicRegion>::const_iterator j(regions.get_first());
	   j != regions.get_last(); ++j, ++k)
	if (j->get_score() <= max_mismatches)
	  add_contribution_g(i, *j, *k, meth_count, unmeth_count);
      const double total = meth_count + unmeth_count;
      out << chrom_name << "\t" << i << "\t" << i + 1 << "\tG:" 
	  << total << "\t" << meth_count/max(1.0, total) << "\t+\n";
    }
  }
}


static void
identify_chromosomes(const bool VERBOSE, const string chrom_file,
		     const string fasta_suffix, vector<string> &chrom_files) {
  if (VERBOSE)
    cerr << "[IDENTIFYING CHROMS] ";
  if (isdir(chrom_file.c_str())) 
    read_dir(chrom_file, fasta_suffix, chrom_files);
  else chrom_files.push_back(chrom_file);
  if (VERBOSE) {
    cerr << "[DONE]" << endl 
	 << "chromosome files found (approx size):" << endl;
    for (vector<string>::const_iterator i = chrom_files.begin();
	 i != chrom_files.end(); ++i)
      cerr << *i << " (" << roundf(get_filesize(*i)/1e06) << "Mbp)" << endl;
    cerr << endl;
  }
}


template <class T> void
advance_chromosome(const GenomicRegion &chrom_region, 
		   FileIterator<GenomicRegion> &regions, 
		   FileIterator<T> &reads) {
  while (regions.last_is_good() && 
	 (*regions.get_last() < chrom_region)) {
    assert(regions.last_is_good());
    regions.increment_last();
    reads.increment_last();
  }
  while (regions.first_is_good() && 
	 (*regions.get_first() < chrom_region)) {
    regions.increment_first();
    reads.increment_first();
  }
}


template <class T> 
void
scan_chroms(const bool VERBOSE, const bool PROCESS_NON_CPGS,
	    const QualityChecker &qc,
	    const double max_mismatches,
	    const string &outfile, const vector<string> &chrom_files, 
	    FileIterator<GenomicRegion> &regions,
	    FileIterator<T> &reads) {
  
  std::ostream *out = (outfile.empty()) ? &cout : new std::ofstream(outfile.c_str());
  for (size_t i = 0; i < chrom_files.size(); ++i) {
    const string fn(strip_path_and_suffix(chrom_files[i]));
    if (VERBOSE)
      cerr << "[LOADING CHROM FILE=" << fn << "]";
    vector<string> chrom_names, chroms;
    read_fasta_file(chrom_files[i].c_str(), chrom_names, chroms);
    for (size_t j = 0; j < chroms.size(); ++j) {
      if (VERBOSE) cerr << "[SCANNING=" << chrom_names[j] << "]";
      //TODO: WHAT HAPPENS IF A CHROM IS MISSING??
      const GenomicRegion chrom_region(chrom_names[j], 0, 0);
      advance_chromosome(chrom_region, regions, reads);
      if (PROCESS_NON_CPGS)
	scan_chromosome(qc, chroms[j], chrom_region, max_mismatches,
			regions, reads, *out);
      else scan_chromosome_cpg(qc, chroms[j], chrom_region, max_mismatches,
			       regions, reads, *out);
    }
    if (VERBOSE) cerr << " [DONE]" << endl;
  }
  if (out != &cout) delete out;
}


template <class T> 
void
scan_chroms(const bool VERBOSE, const bool PROCESS_NON_CPGS,
	    const double max_mismatches,
	    const string &outfile, const vector<string> &chrom_files, 
	    FileIterator<GenomicRegion> &regions,
	    FileIterator<T> &reads) {
  std::ostream *out = (outfile.empty()) ? &cout : new std::ofstream(outfile.c_str());
  for (size_t i = 0; i < chrom_files.size(); ++i) {
    const string fn(strip_path_and_suffix(chrom_files[i]));
    if (VERBOSE)
      cerr << "[LOADING CHROM FILE=" << fn << "]";
    vector<string> chrom_names, chroms;
    read_fasta_file(chrom_files[i].c_str(), chrom_names, chroms);
    for (size_t j = 0; j < chroms.size(); ++j) {
      if (VERBOSE) cerr << "[SCANNING=" << chrom_names[j] << "]";
      //TODO: WHAT HAPPENS IF A CHROM IS MISSING??
      const GenomicRegion chrom_region(chrom_names[j], 0, 0);
      advance_chromosome(chrom_region, regions, reads);
      if (PROCESS_NON_CPGS)
	scan_chromosome(chroms[j], chrom_region, max_mismatches,
			regions, reads, *out);
      else scan_chromosome_cpg(chroms[j], chrom_region, max_mismatches,
			       regions, reads, *out);
    }
    if (VERBOSE) cerr << " [DONE]" << endl;
  }
  if (out != &cout) delete out;
}



int 
main(int argc, const char **argv) {
  
  try {
    
    bool VERBOSE = false;
    bool PROCESS_NON_CPGS = false;
    
    string mapped_file;
    string chrom_file;
    string outfile;
    string fasta_suffix = "fa";
    
    size_t BUFFER_SIZE = 100000;
    double max_mismatches = std::numeric_limits<double>::max();

    double cutoff = -std::numeric_limits<double>::max();
    
    /****************** COMMAND LINE OPTIONS ********************/
    OptionParser opt_parse(argv[0], "a program for counting the "
			   "methylated and unmethylated reads mapping "
			   "over each CpG or C.",
			   "<fast[a/q]-reads>");
    opt_parse.add_opt("output", 'o', "Name of output file (default: stdout)", 
		      false, outfile);
    opt_parse.add_opt("chrom", 'c', "FASTA file or dir containing chromosome(s)", 
		      true , chrom_file);
    opt_parse.add_opt("suffix", 's', "suffix of FASTA files "
		      "(assumes -c indicates dir)", 
		      false , fasta_suffix);
    opt_parse.add_opt("mapped", 'm', "file of mapped locations", 
		      true , mapped_file);
    opt_parse.add_opt("non", 'N', "process non-CpG cytosines", 
		      false , PROCESS_NON_CPGS);
    opt_parse.add_opt("buffer", 'B', "buffer size (in records, not bytes)", 
		      false , BUFFER_SIZE);
    opt_parse.add_opt("max", 'M', "max mismatches (can be fractional)", 
		      false , max_mismatches);
    opt_parse.add_opt("cutoff", 'C', "cutoff for high-quality bases (assumes fastq reads)", 
		      false , cutoff);
    opt_parse.add_opt("verbose", 'v', "print more run info", false, VERBOSE);
    vector<string> leftover_args;
    opt_parse.parse(argc, argv, leftover_args);
    if (argc == 1 || opt_parse.help_requested()) {
      cerr << opt_parse.help_message() << endl
	   << opt_parse.about_message() << endl;
      return EXIT_SUCCESS;
    }
    if (opt_parse.about_requested()) {
      cerr << opt_parse.about_message() << endl;
      return EXIT_SUCCESS;
    }
    if (opt_parse.option_missing()) {
      cerr << opt_parse.option_missing_message() << endl;
      return EXIT_SUCCESS;
    }
    if (leftover_args.empty()) {
      cerr << opt_parse.help_message() << endl;
      return EXIT_SUCCESS;
    }
    const string reads_file = leftover_args.front();
    /****************** END COMMAND LINE OPTIONS *****************/

    if (VERBOSE)
      cerr << "MAX MISMATCHES=" << max_mismatches << endl;

    const bool FASTQ = is_fastq(reads_file);
    if (VERBOSE)
      cerr << "READS FILE FORMAT: " << ((FASTQ) ? "FASTQ" : "FASTA") << endl;

    FASTQScoreType score_format = (FASTQ) ?
      fastq_score_type(reads_file) : FASTQ_Solexa;

    if (VERBOSE && FASTQ)
      cerr << "SCORE FORMAT: " 
	   << ((FASTQScoreIsPhred(score_format)) ? "Phred" : "Solexa") << endl;
    
    vector<string> chrom_files;
    identify_chromosomes(VERBOSE, chrom_file, fasta_suffix, chrom_files);
    sort(chrom_files.begin(), chrom_files.end());
    
    FileIterator<GenomicRegion> regions(mapped_file, BUFFER_SIZE);
    if (FASTQ) {
      FileIterator<FASTQRecord> reads(reads_file, BUFFER_SIZE);
      if (cutoff != -std::numeric_limits<double>::max()) {
	const QualityChecker qc(score_format, cutoff);
	scan_chroms(VERBOSE, PROCESS_NON_CPGS, qc, max_mismatches, 
		    outfile, chrom_files, regions, reads);
      }
      else scan_chroms(VERBOSE, PROCESS_NON_CPGS, max_mismatches, 
		       outfile, chrom_files, regions, reads);
    }
    else {
      FileIterator<string> reads(reads_file, BUFFER_SIZE);
      scan_chroms(VERBOSE, PROCESS_NON_CPGS, max_mismatches, 
		  outfile, chrom_files, regions, reads);
    }
  }
  catch (const RMAPException &e) {
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  }
  catch (std::bad_alloc &ba) {
    cerr << "ERROR: could not allocate memory" << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
